#include "ImguiOverlayPass.h"

ImguiOverlayPass::ImguiOverlayPass() {
    PassName = "ImguiOverlayPass";
}

ImguiOverlayPass::~ImguiOverlayPass() {
}

void ImguiOverlayPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    // Snapshot composition's output so the viewport widget can sample it while we overlay
    // ImGui's draws back into frameOutput — avoids a read/write feedback hazard.
    {
        VkImageMemoryBarrier preCopy[2] = {};
        preCopy[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        preCopy[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        preCopy[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        preCopy[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopy[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopy[0].image = app->frameOutput;
        preCopy[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        preCopy[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        preCopy[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        preCopy[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        preCopy[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        preCopy[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        preCopy[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopy[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopy[1].image = app->frameOutputSnapshot;
        preCopy[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        preCopy[1].srcAccessMask = 0;
        preCopy[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 2, preCopy);
    }

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { app->swapChainExtent.width, app->swapChainExtent.height, 1 };

    vkCmdCopyImage(commandBuffer,
        app->frameOutput, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        app->frameOutputSnapshot, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion);

    {
        VkImageMemoryBarrier postCopy[2] = {};
        postCopy[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postCopy[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postCopy[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postCopy[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postCopy[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postCopy[0].image = app->frameOutput;
        postCopy[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        postCopy[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postCopy[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        postCopy[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postCopy[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        postCopy[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        postCopy[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postCopy[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postCopy[1].image = app->frameOutputSnapshot;
        postCopy[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        postCopy[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postCopy[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 2, postCopy);
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = app->frameOutputView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset = { 0, 0 };
    renderInfo.renderArea.extent = app->swapChainExtent;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(commandBuffer, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRendering(commandBuffer);
}
