#pragma once
#include "RenderPassObject.h"
class AlbedoPass : public RenderPassObject
{
public:
    std::vector<std::string> PassNames;
    // Constructor
    AlbedoPass();

    // Destructor
    ~AlbedoPass();

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void CreateImages() override;
};