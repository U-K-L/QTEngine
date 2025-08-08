#include "CompositionPass.h"
static bool capturing = false;
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

    if (GetKeyState('1') & 0x8000)
    {
        std::cout << "Final Render View" << std::endl;
        pc.input = 0;
    }
    else if (GetKeyState('2') & 0x8000)
    {
        std::cout << "Normals SDF Render View" << std::endl;
        pc.input = 1;
    }
    else if (GetKeyState('3') & 0x8000)
    {
        std::cout << "SDF Normals Render View" << std::endl;
        pc.input = 2;
    }
    else if (GetKeyState('4') & 0x8000)
    {
        std::cout << "Full SDF Render View" << std::endl;
        pc.input = 3;
    }
    else if (GetKeyState('5') & 0x8000)
    {
        std::cout << "Dual Contour Render View" << std::endl;
        pc.input = 4;
    }
    else if (GetKeyState('6') & 0x8000)
    {
        std::cout << "Depth Render View" << std::endl;
        pc.input = 5;
    }
    else if (GetKeyState('7') & 0x8000)
    {
        std::cout << "Raster Render View" << std::endl;
        pc.input = 6;
    }

    if (GetKeyState('R') & 0x8000 && capturing == false)
    {
        //Empty string for default output path.
        app->StartRecording("", 30);
        capturing = true;
    }
    else if (GetKeyState('S') & 0x8000 && capturing == true)
	{
		app->StopRecording();
		capturing = false;
	}


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
    material.textureNames[9] = "CombineSDFRasterPass";
    material.textureNames[10] = "FullSDFField";

    //material.textures.push_back(UnigmaTexture("animeGirl"));
    //material.textures[0].TEXTURE_PATH = "Assets/Textures/animeGirl.png";

}

//Used to create final image.
void CompositionPass::CreateImages() {
    QTDoughApplication* app = QTDoughApplication::instance;
    RenderPassObject::CreateImages();
}