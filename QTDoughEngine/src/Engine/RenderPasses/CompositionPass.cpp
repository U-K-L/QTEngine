#include "CompositionPass.h"

CompositionPass::~CompositionPass() {
}

CompositionPass::CompositionPass() {
    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

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
    material.textureNames[1] = "AlbedoPass2";
    material.textureNames[2] = "NormalPass";
    material.textureNames[3] = "PositionPass";
    material.textureNames[4] = "DepthPass";
    material.textureNames[5] = "OutlinePass";
    material.textureNames[6] = "SDFAlbedoPass";
    material.textureNames[7] = "SDFNormalPass";
    material.textureNames[8] = "SDFPositionPass";

    //material.textures.push_back(UnigmaTexture("animeGirl"));
    //material.textures[0].TEXTURE_PATH = "Assets/Textures/animeGirl.png";

}

//Used to create final image.
void CompositionPass::CreateImages() {
    QTDoughApplication* app = QTDoughApplication::instance;
    RenderPassObject::CreateImages();
}