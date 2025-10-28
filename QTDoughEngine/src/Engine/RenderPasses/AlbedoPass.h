#pragma once
#include "RenderPassObject.h"
class AlbedoPass : public RenderPassObject
{
public:

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec4 baseAlbedo;
        alignas(16) glm::vec2 texelSize;
        alignas(16) glm::vec4 outerOutlineColor;
        alignas(16) glm::vec4 innerOutlineColor;
        Uint32 ID;
    };

    // Constructor
    AlbedoPass();

    // Destructor
    ~AlbedoPass();

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void CreateImages() override;
    void RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, UnigmaCameraStruct* CameraMain) override;

};