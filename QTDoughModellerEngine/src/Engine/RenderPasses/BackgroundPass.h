#pragma once
#include "RenderPassObject.h"
class BackgroundPass : public RenderPassObject
{
public:
    // Constructor
    BackgroundPass();

    // Destructor
    ~BackgroundPass();

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage) override;
};