#include "BackgroundPass.h"

BackgroundPass::~BackgroundPass() {
}

BackgroundPass::BackgroundPass() {}

void BackgroundPass::CreateMaterials() {
    //Add in textures.
    material.textures.push_back(UnigmaTexture( std::string(AssetsPath + "Textures/SkyboxBorder.png") ));
    material.shader = UnigmaShader("background");

}