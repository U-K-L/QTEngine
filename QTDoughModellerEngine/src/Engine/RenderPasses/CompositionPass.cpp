#include "CompositionPass.h"

CompositionPass::~CompositionPass() {
}

CompositionPass::CompositionPass() {}

void CompositionPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkImageView* targetImage)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    RenderPassObject::Render(commandBuffer, imageIndex, &app->swapChainImageViews[imageIndex]);
}

void CompositionPass::CreateMaterials() {
    //Add in textures.
    material.textures.push_back(UnigmaTexture(std::string(AssetsPath + "Textures/SkyboxBorder.png")));
    material.shader = UnigmaShader("background");

}