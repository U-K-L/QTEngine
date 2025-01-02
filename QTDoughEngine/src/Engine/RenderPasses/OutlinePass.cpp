#include "OutlinePass.h"

OutlinePass::~OutlinePass() {
    PassName = "OutlinePass";
}

OutlinePass::OutlinePass() {
    PassName = "OutlinePass";
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

    material.textureNames[0] = "NormalPass";
    material.textureNames[1] = "PositionPass";
    material.textureNames[2] = "DepthPass";


}