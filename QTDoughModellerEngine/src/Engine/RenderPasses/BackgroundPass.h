#pragma once
#include "RenderPassObject.h"
class BackgroundPass : public RenderPassObject
{
public:
    // Constructor
    BackgroundPass();

    // Destructor
    ~BackgroundPass();

    // Optional: Override base class methods
    void CreateDescriptorPool() override;
    void CreateDescriptorSets() override;
    void CreateGraphicsPipeline() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex) override;
    void CreateImages() override;
    void CreateDescriptorSetLayout() override;
    void CreateMaterials() override;
};