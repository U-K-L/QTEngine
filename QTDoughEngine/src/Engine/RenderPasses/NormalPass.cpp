#include "NormalPass.h"
#include "VoxelizerPass.h"

NormalPass::~NormalPass() {
    PassName = "NormalPass";
}

NormalPass::NormalPass() {
    PassName = "NormalPass";

    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

void NormalPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("normal");

}

void NormalPass::AddObjects(UnigmaRenderingObject* unigmaRenderingObjects)
{

    QTDoughApplication* app = QTDoughApplication::instance;
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (i == 1)
            continue;
        if (unigmaRenderingObjects[i].isRendering)
        {
            renderingObjects.push_back(&unigmaRenderingObjects[i]);
        }
    }
}

void NormalPass::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) {
    RenderPerObject(commandBuffer, imageIndex, currentFrame, targetImage, CameraMain);
}

void NormalPass::RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, UnigmaCameraStruct* CameraMain)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VoxelizerPass* voxelizer = VoxelizerPass::instance;

    if(voxelizer->dispatchCount < 10)
        return;

    for (int i = 0; i < renderingObjects.size(); i++)
    {
        if (renderingObjects[i]->isRendering)
        {
            renderingObjects[i]->UpdateUniformBuffer(*app, currentFrame, *renderingObjects[i], *CameraMain, _uniformBuffersMapped[currentFrame]);

            renderingObjects[i]->RenderBrush(*app, commandBuffer, imageIndex, currentFrame, graphicsPipeline, pipelineLayout, descriptorSets[currentFrame], voxelizer->meshingVertexBuffer, voxelizer->readBackVertexCount, voxelizer->indirectDrawBuffer);
        }
    }
}