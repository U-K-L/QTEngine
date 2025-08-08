#include "VideoRecorder.h"
#include <stdexcept>
#include <cstring>
#include <cstdio>

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }
    // fall back to any host-visible type if requested one is missing
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            return i;
    }
    return 0;
}

VideoRecorder::VideoRecorder(VkDevice device,
    VkPhysicalDevice phys,
    VkQueue graphicsQueue,
    VkCommandPool commandPool)
    : device_(device), phys_(phys), queue_(graphicsQueue), cmdPool_(commandPool) {}

VideoRecorder::~VideoRecorder() {
    End();
    DestroyStagingRing();
}

const char* VideoRecorder::FfmpegPixelFmtFor(VkFormat fmt) {
    switch (fmt) {
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM: return "bgra";
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM: return "rgba";
    default: return "bgra";
    }
}
bool VideoRecorder::IsBGRA(VkFormat f) { return f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_B8G8R8A8_UNORM; }
bool VideoRecorder::IsRGBA(VkFormat f) { return f == VK_FORMAT_R8G8B8A8_SRGB || f == VK_FORMAT_R8G8B8A8_UNORM; }

bool VideoRecorder::OpenFfmpeg(const char* outPath, uint32_t w, uint32_t h, uint32_t fps, const char* pixfmt) {
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pix_fmt %s -s %ux%u -r %u -i - "
        "-an -vcodec libx264 -preset veryfast -crf 18 -pix_fmt yuv420p \"%s\"",
        pixfmt, w, h, fps, outPath);
        #ifdef _WIN32
            pipe_ = _popen(cmd, "wb");
        #else
            pipe_ = popen(cmd, "w");
        #endif
            return pipe_ != nullptr;
        }
        void VideoRecorder::CloseFfmpeg() {
            if (!pipe_) return;
        #ifdef _WIN32
            _pclose(pipe_);
        #else
            pclose(pipe_);
        #endif
            pipe_ = nullptr;
}

bool VideoRecorder::CreateStagingRing(uint32_t frames, VkDeviceSize bytes) {
    ring_.resize(frames);
    for (uint32_t i = 0; i < frames; ++i) {
        // Buffer
        VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bi.size = bytes;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &bi, nullptr, &ring_[i].buffer);

        // Mem reqs
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device_, ring_[i].buffer, &req);

        // Allocate
        VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryTypeIndex(phys_, req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &ai, nullptr, &ring_[i].memory);

        // Bind
        vkBindBufferMemory(device_, ring_[i].buffer, ring_[i].memory, 0);

        ring_[i].size = bytes;
        ring_[i].ready = false;
    }
    return true;
}

void VideoRecorder::DestroyStagingRing() {
    for (auto& s : ring_) {
        if (s.buffer) vkDestroyBuffer(device_, s.buffer, nullptr);
        if (s.memory) vkFreeMemory(device_, s.memory, nullptr);
    }
    ring_.clear();
}

bool VideoRecorder::Begin(const char* outputPath,
    uint32_t width,
    uint32_t height,
    uint32_t fps,
    VkFormat swapchainFormat,
    uint32_t maxFramesInFlight)
{
    if (recording_) return true;
    width_ = width; height_ = height; fps_ = fps;
    srcFormat_ = swapchainFormat;
    ffPixFmt_ = FfmpegPixelFmtFor(srcFormat_);
    ringSize_ = maxFramesInFlight;

    const VkDeviceSize bytes = VkDeviceSize(width_) * VkDeviceSize(height_) * 4;
    if (!CreateStagingRing(ringSize_, bytes)) return false;
    if (!OpenFfmpeg(outputPath, width_, height_, fps_, ffPixFmt_)) return false;

    recording_ = true;
    return true;
}

void VideoRecorder::End() {
    if (!recording_) return;
    CloseFfmpeg();
    DestroyStagingRing();
    recording_ = false;
}

void VideoRecorder::CmdCopySwapImageToStaging(VkCommandBuffer cmd,
    VkImage srcSwapImage,
    uint32_t frameIndex,
    uint32_t width,
    uint32_t height)
{
    if (!recording_) return;

    VkImageMemoryBarrier toCopy{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    toCopy.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toCopy.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.image = srcSwapImage;
    toCopy.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toCopy.subresourceRange.baseMipLevel = 0;
    toCopy.subresourceRange.levelCount = 1;
    toCopy.subresourceRange.baseArrayLayer = 0;
    toCopy.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toCopy);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(cmd,
        srcSwapImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        ring_[frameIndex].buffer, 1, &region);

    VkImageMemoryBarrier back{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    back.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    back.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    back.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    back.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    back.image = srcSwapImage;
    back.subresourceRange = toCopy.subresourceRange;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &back);

    ring_[frameIndex].ready = true;
}

void VideoRecorder::DrainFrameToEncoder(uint32_t frameIndex) {
    if (!recording_ || !pipe_) return;
    auto& s = ring_[frameIndex];
    if (!s.ready) return;

    void* mapped = nullptr;
    vkMapMemory(device_, s.memory, 0, s.size, 0, &mapped);
    std::fwrite(mapped, 1, static_cast<size_t>(s.size), pipe_);
    vkUnmapMemory(device_, s.memory);

    s.ready = false;
}
