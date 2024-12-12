#include "PositionPass.h"

PositionPass::~PositionPass() {
    PassName = "PositionPass";
}

PositionPass::PositionPass() {
    PassName = "PositionPass";
}

void PositionPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("position");

}

void PositionPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) {
    RenderPerObject(commandBuffer, imageIndex, currentFrame, targetImage, CameraMain);
}


void PositionPass::RenderPerObject(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) {
    QTDoughApplication* app = QTDoughApplication::instance;

    if (targetImage == nullptr)
        targetImage = &imageView;



    // **Dynamic Rendering Setup**
    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 0.0f}} };
    VkClearValue depthClear = { {1.0f, 0} };

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = *targetImage;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clearColor;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue = depthClear;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = app->swapChainExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional

    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional


    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // Combine both global and per-object descriptor sets
    VkDescriptorSet descriptorSetsToBind[] = {
        app->globalDescriptorSet,       // Set 0: Global descriptor set
        descriptorSets[currentFrame]    // Set 1: Per-object descriptor set
    };


    // Bind the global descriptor set
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, // First set
        2, // Number of sets
        descriptorSetsToBind,
        0, nullptr // No dynamic offsets
    );


    // Set viewport and scissor if dynamic
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(app->swapChainExtent.width);
    viewport.height = static_cast<float>(app->swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = app->swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    RenderObjects(commandBuffer, imageIndex, currentFrame, targetImage, CameraMain);

    vkCmdEndRendering(commandBuffer);

    // Transition the image layout to shader read-only optimal
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    VkImageMemoryBarrier depthBarrier{};
    depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.image = depthImage;
    depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthBarrier.subresourceRange.baseMipLevel = 0;
    depthBarrier.subresourceRange.levelCount = 1;
    depthBarrier.subresourceRange.baseArrayLayer = 0;
    depthBarrier.subresourceRange.layerCount = 1;
    depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,     
        0,
        0, nullptr,
        0, nullptr,
        1, &depthBarrier
    );
}

void PositionPass::CreateImages() {
    QTDoughApplication* app = QTDoughApplication::instance;

    for (int i = 0; i < material.textures.size(); i++) {
        app->LoadTexture(material.textures[i].TEXTURE_PATH);
    }

    VkFormat imageFormat = app->_swapChainImageFormat; // Use the same format as swap chain images

    // Create the offscreen image
    app->CreateImage(
        app->swapChainExtent.width,
        app->swapChainExtent.height,
        imageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image,
        imageMemory
    );

    // Transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0; // No access before this
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Ready for transfer

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   // Earliest possible pipeline stage
        VK_PIPELINE_STAGE_TRANSFER_BIT,     // Transition happens before transfer
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    app->EndSingleTimeCommands(commandBuffer);

    // Create the image view for the offscreen image
    imageView = app->CreateImageView(image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    //Create the depth image
    VkFormat depthFormat = app->FindDepthFormat();

    app->CreateImage(app->swapChainExtent.width, app->swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = app->CreateImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Create the framebuffer for offscreen rendering
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = VK_NULL_HANDLE; // We'll use dynamic rendering
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &imageView;
    framebufferInfo.width = app->swapChainExtent.width;
    framebufferInfo.height = app->swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(app->_logicalDevice, &framebufferInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen framebuffer!");
    }

    // After rendering, transition the image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    commandBuffer = app->BeginSingleTimeCommands();

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Written by transfer
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;    // Ready for shader read

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,           // After transfer completes
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,    // Before shader reads
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    app->EndSingleTimeCommands(commandBuffer);

    // Store the offscreen texture for future references
    UnigmaTexture offscreenTexture;
    offscreenTexture.u_image = image;
    offscreenTexture.u_imageView = imageView;
    offscreenTexture.u_imageMemory = imageMemory;
    offscreenTexture.TEXTURE_PATH = PassName;

    // Use a unique key for the offscreen image
    std::string textureKey = PassName;
    app->textures.insert({ textureKey, offscreenTexture });


    //Add depth to textures.
    UnigmaTexture offscreenDepthTexture;
    offscreenDepthTexture.u_image = depthImage;
    offscreenDepthTexture.u_imageView = depthImageView;
    offscreenDepthTexture.u_imageMemory = depthImageMemory;
    offscreenDepthTexture.TEXTURE_PATH = "DepthPass";

    // Use a unique key for the offscreen image
    std::string textureKey2 = "DepthPass";
    app->textures.insert({ textureKey2, offscreenDepthTexture });
}

void PositionPass::CleanupPipeline() {
    VkDevice device = QTDoughApplication::instance->_logicalDevice;

    std::cout << "Cleaning up pipeline" << std::endl;

    if (offscreenFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, offscreenFramebuffer, nullptr);
        offscreenFramebuffer = VK_NULL_HANDLE;
    }

    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    //delete images
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, nullptr);
    }
    if (imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, imageMemory, nullptr);
    }
    //image view
    if (imageView != VK_NULL_HANDLE)
        vkDestroyImageView(device, imageView, nullptr);

    //depth images
    if (depthImage != VK_NULL_HANDLE) {
		vkDestroyImage(device, depthImage, nullptr);
	}
    if (depthImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, depthImageMemory, nullptr);
	}
    if (depthImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device, depthImageView, nullptr);
	}


}
