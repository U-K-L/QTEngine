#pragma once
#include "RenderPassObject.h"

class ImguiOverlayPass : public RenderPassObject
{
public:
    ImguiOverlayPass();
    ~ImguiOverlayPass();

    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;

    void CreateImages() override {}
    void CreateMaterials() override {}
    void CreateDescriptorSetLayout() override {}
    void CreateDescriptorPool() override {}
    void CreateDescriptorSets() override {}
    void CreateUniformBuffers() override {}
    void CreateGraphicsPipeline() override {}
    void UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame) override {}
    void CleanupPipeline() override {}
};
