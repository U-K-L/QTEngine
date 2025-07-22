#include "OutlinePass.h"

OutlinePass::~OutlinePass() {
    PassName = "OutlinePass";
}

OutlinePass::OutlinePass() {
    PassName = "OutlinePass";

    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

void OutlinePass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    RenderPassObject::Render(commandBuffer, imageIndex, currentFrame, targetImage);
}

void OutlinePass::CreateMaterials() {
    material.Clean();
    //Add in textures.
    material.shader = UnigmaShader("outline");

    material.textureNames[0] = "SDFNormalPass";
    material.textureNames[1] = "SDFPositionPass";
    material.textureNames[2] = "DepthPass";
    material.textureNames[3] = "AlbedoPass2";
    material.textureNames[4] = "OutlineColorsPass";
    material.textureNames[5] = "InnerOutlineColorsPass";
    material.textureNames[6] = "UVPass";
    material.textureNames[7] = "NoiseAndGrain";

    material.push_back_texture(UnigmaTexture(std::string(AssetsPath + "Textures/NoiseAndGrain.png")), std::string(AssetsPath + "Textures/NoiseAndGrain.png"));



}