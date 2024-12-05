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
