#pragma once
#include "RenderPassObject.h"
class NormalPass : public RenderPassObject
{
public:
    // Constructor
    NormalPass();

    // Destructor
    ~NormalPass();

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void AddObjects(UnigmaRenderingObject* unigmaRenderingObjects) override;
    void RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, UnigmaCameraStruct* CameraMain) override;
};