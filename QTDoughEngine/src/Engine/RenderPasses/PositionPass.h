#pragma once
#include "RenderPassObject.h"
class PositionPass : public RenderPassObject
{
public:
    // Constructor
    PositionPass();

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // Destructor
    ~PositionPass();

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void PositionPass::RenderPerObject(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain) override;
    void CreateImages() override;
    void CleanupPipeline() override;

};