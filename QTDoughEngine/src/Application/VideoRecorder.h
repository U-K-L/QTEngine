#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdio>
#include <cstdint>

class VideoRecorder {
public:
    VideoRecorder(VkDevice device,
        VkPhysicalDevice phys,
        VkQueue graphicsQueue,
        VkCommandPool commandPool);
    ~VideoRecorder();

    bool Begin(const char* outputPath,
        uint32_t width,
        uint32_t height,
        uint32_t fps,
        VkFormat swapchainFormat,
        uint32_t maxFramesInFlight);

    bool IsRecording() const { return recording_; }

    void CmdCopySwapImageToStaging(VkCommandBuffer cmd,
        VkImage srcSwapImage,
        uint32_t frameIndex,
        uint32_t width,
        uint32_t height);



    void DrainFrameToEncoder(uint32_t frameIndex);
    void End();

private:
    struct Staging {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        bool ready = false;
    };

    bool CreateStagingRing(uint32_t frames, VkDeviceSize bytes);
    void DestroyStagingRing();

    static const char* FfmpegPixelFmtFor(VkFormat fmt);
    static bool IsBGRA(VkFormat fmt);
    static bool IsRGBA(VkFormat fmt);

    bool OpenFfmpeg(const char* outPath, uint32_t w, uint32_t h, uint32_t fps, const char* pixfmt);
    void CloseFfmpeg();

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;

    std::vector<Staging> ring_;
    uint32_t width_ = 0, height_ = 0, fps_ = 0, ringSize_ = 0;

    VkFormat srcFormat_ = VK_FORMAT_UNDEFINED;
    const char* ffPixFmt_ = "bgra";
    bool recording_ = false;

#ifdef _WIN32
    FILE* pipe_ = nullptr;
#else
    FILE* pipe_ = nullptr;
#endif
};
