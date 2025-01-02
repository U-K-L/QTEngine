#pragma once
#include "RenderPassObject.h"
class OutlinePass : public RenderPassObject
{
public:
    // Constructor
    OutlinePass();

    // Destructor
    ~OutlinePass();

    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void CreateMaterials() override;
};