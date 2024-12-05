#pragma once
#include "RenderPassObject.h"
class PositionPass : public RenderPassObject
{
public:
    // Constructor
    PositionPass();

    // Destructor
    ~PositionPass();

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
};