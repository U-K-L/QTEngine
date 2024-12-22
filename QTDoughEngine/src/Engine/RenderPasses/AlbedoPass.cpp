#include "AlbedoPass.h"

AlbedoPass::~AlbedoPass() {
    PassName = "AlbedoPass";
}

AlbedoPass::AlbedoPass() {
    PassName = "AlbedoPass";
}

void AlbedoPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("albedo");

    material.textureNames[0] = "animeGirl";
}

void AlbedoPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) {
    RenderPerObject(commandBuffer, imageIndex, currentFrame, targetImage, CameraMain);
}
