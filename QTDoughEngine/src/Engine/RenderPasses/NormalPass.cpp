#include "NormalPass.h"

NormalPass::~NormalPass() {
    PassName = "NormalPass";
}

NormalPass::NormalPass() {
    PassName = "NormalPass";
}

void NormalPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("normal");

}

void NormalPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) {
    RenderPerObject(commandBuffer, imageIndex, currentFrame, targetImage, CameraMain);
}
