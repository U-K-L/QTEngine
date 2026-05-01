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

    // View mode from editor UI tabs (or fallback to keyboard shortcuts)
    pc.input = (int)app->editorState.viewMode;

    if (GetKeyState('1') & 0x8000)      { pc.input = (int)ViewModes::Render; app->editorState.viewMode = ViewModes::Render; }
    else if (GetKeyState('2') & 0x8000) { pc.input = (int)ViewModes::SDF; app->editorState.viewMode = ViewModes::SDF; }
    else if (GetKeyState('3') & 0x8000) { pc.input = (int)ViewModes::Normals; app->editorState.viewMode = ViewModes::Normals; }
    else if (GetKeyState('4') & 0x8000) { pc.input = 3; app->editorState.viewMode = ViewModes::Normals; }
    else if (GetKeyState('5') & 0x8000) { pc.input = (int)ViewModes::Albedo; app->editorState.viewMode = ViewModes::Albedo; }
    else if (GetKeyState('6') & 0x8000) { pc.input = 5; app->editorState.viewMode = ViewModes::Albedo; }
    else if (GetKeyState('7') & 0x8000) { pc.input = 6; app->editorState.viewMode = ViewModes::Material; }
    else if (GetKeyState('8') & 0x8000) { pc.input = (int)ViewModes::MaterialBrush; app->editorState.viewMode = ViewModes::MaterialBrush; }
    else if (GetKeyState('9') & 0x8000) { pc.input = (int)ViewModes::Quanta; app->editorState.viewMode = ViewModes::Quanta; }

    // Editor: render to offscreen viewport image. Play: render to swapchain.
    VkImageView* target = app->editorState.IsEditor()
        ? &app->editorState.viewportImageView
        : &app->swapChainImageViews[imageIndex];
    RenderPassObject::Render(commandBuffer, imageIndex, currentFrame, target);
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
    material.textureNames[9] = "CombineSDFRasterPass";
    material.textureNames[10] = "FullSDFField";
    material.textureNames[11] = "RayAlbedoPass";
    material.textureNames[12] = "RayNormalPass";
    material.textureNames[13] = "RayPositionPass";
    material.textureNames[14] = "MaterialGridPass";
    material.textureNames[15] = "QuantaSpherePass";

    //material.textures.push_back(UnigmaTexture("animeGirl"));
    //material.textures[0].TEXTURE_PATH = "Assets/Textures/animeGirl.png";

}

//Used to create final image.
void CompositionPass::CreateImages() {
    QTDoughApplication* app = QTDoughApplication::instance;
    RenderPassObject::CreateImages();
}