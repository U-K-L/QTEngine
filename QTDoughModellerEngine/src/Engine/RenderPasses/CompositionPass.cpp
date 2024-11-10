#include "CompositionPass.h"

CompositionPass::~CompositionPass() {
}

CompositionPass::CompositionPass() {}

void CompositionPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    RenderPassObject::Render(commandBuffer, imageIndex, currentFrame, &app->swapChainImageViews[imageIndex]);
}

void CompositionPass::CreateMaterials() {
    material.Clean();
    //Add in textures.
    material.shader = UnigmaShader("composition");

    material.textureNames[0] = "BackgroundPass";

}