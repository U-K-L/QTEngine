#include "CompositionPass.h"

CompositionPass::~CompositionPass() {
}

CompositionPass::CompositionPass() {}

void CompositionPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    RenderPassObject::Render(commandBuffer, imageIndex, currentFrame, &app->swapChainImageViews[imageIndex]);
}

void CompositionPass::CreateMaterials() {
    material.Clean();
    //Add in textures.
    material.shader = UnigmaShader("composition");

    material.textureNames[0] = "BackgroundPass";
    material.textureNames[1] = "AlbedoPass";
    material.textureNames[2] = "NormalPass";
    material.textureNames[3] = "PositionPass";
    material.textureNames[4] = "DepthPass";

    //material.textureNames[5] = "animeGirl";

    //material.textures.push_back(UnigmaTexture("animeGirl"));
    //material.textures[0].TEXTURE_PATH = "Assets/Textures/animeGirl.png";

}