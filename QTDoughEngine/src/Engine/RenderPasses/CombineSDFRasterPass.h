#pragma once
#include "RenderPassObject.h"
class CombineSDFRasterPass : public RenderPassObject
{
public:
    // Constructor
    CombineSDFRasterPass();

    // Destructor
    ~CombineSDFRasterPass();

    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void CreateMaterials() override;
    void CreateImages() override;

};