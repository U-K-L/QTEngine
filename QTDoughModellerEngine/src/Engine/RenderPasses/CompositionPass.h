#pragma once
#include "RenderPassObject.h"
class CompositionPass : public RenderPassObject
{
public:
    // Constructor
    CompositionPass();

    // Destructor
    ~CompositionPass();

    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkImageView* targetImage) override;
    void CreateMaterials() override;
};