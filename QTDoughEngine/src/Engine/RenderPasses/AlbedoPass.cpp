#include "AlbedoPass.h"

AlbedoPass::~AlbedoPass() {
    PassName = "AlbedoPass";
}

AlbedoPass::AlbedoPass(VoxelizerPass* ivoxelizer) {
    PassName = "AlbedoPass";
    voxelizer = ivoxelizer;
    PassNames.push_back("AlbedoPass2");
    PassNames.push_back("OutlineColorsPass");
    PassNames.push_back("InnerOutlineColorsPass");
    PassNames.push_back("UVPass");

    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

void AlbedoPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("albedo");

    material.textureNames[0] = "animeGirl";
}

void AlbedoPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) {
    //Create vector of image views pointers.
    std::vector<VkImageView*> imagesViewsPointers;
    QTDoughApplication* app = QTDoughApplication::instance;
    
    for(int i = 0; i < imagesViews.size(); i++)
	{
		imagesViewsPointers.push_back(&imagesViews[i]);
	}

    RenderPerObject(commandBuffer, imageIndex, currentFrame, imagesViewsPointers, CameraMain);

    // Bind the pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // Bind the vertex buffer
    VkBuffer vertexBuffers[] = { voxelizer->vertexBufferReadbackBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdDrawIndirect(
        commandBuffer,
        voxelizer->indirectDrawBuffer,
        0,
        1,
        sizeof(VkDrawIndirectCommand)
    );
}

void AlbedoPass::CreateImages() {
    QTDoughApplication* app = QTDoughApplication::instance;

    images.resize(PassNames.size());
    imagesViews.resize(PassNames.size());
    imagesMemory.resize(PassNames.size());


    for (int i = 0; i < material.textures.size(); i++) {
        app->LoadTexture(material.textures[i].TEXTURE_PATH);
    }

    //Load all textures for all objects.
    for (int i = 0; i < renderingObjects.size(); i++)
    {
        for (int j = 0; j < renderingObjects[i]->_material.textures.size(); j++)
        {
            //print path
            std::cout << "Loading texture from file folders: " << renderingObjects[i]->_material.textures[j].TEXTURE_PATH << std::endl;
            app->LoadTexture(renderingObjects[i]->_material.textures[j].TEXTURE_PATH);
        }
    }

    for(int i = 0; i < images.size(); i++)
	{
        VkFormat imageFormat = app->_swapChainImageFormat;

        // Create the offscreen image
        app->CreateImage(
            app->swapChainExtent.width,
            app->swapChainExtent.height,
            imageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            images[i],
            imagesMemory[i]
        );

        VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        app->EndSingleTimeCommands(commandBuffer);

        // Create the image view for the offscreen image
        imagesViews[i] = app->CreateImageView(images[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        commandBuffer = app->BeginSingleTimeCommands();

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        app->EndSingleTimeCommands(commandBuffer);

        // Store the offscreen texture for future references
        UnigmaTexture offscreenTexture;
        offscreenTexture.u_image = images[i];
        offscreenTexture.u_imageView = imagesViews[i];
        offscreenTexture.u_imageMemory = imagesMemory[i];
        offscreenTexture.TEXTURE_PATH = PassNames[i];

        // Use a unique key for the offscreen image
        std::string textureKey = PassNames[i];
        app->textures.insert({ textureKey, offscreenTexture });
	}

}