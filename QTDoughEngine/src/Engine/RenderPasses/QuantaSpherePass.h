#pragma once
#include "RenderPassObject.h"

class QuantaSpherePass : public RenderPassObject
{
public:
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec4 midtone;
        alignas(16) glm::vec4 shadow;
        alignas(16) glm::vec4 highlight;
        alignas(16) glm::vec2 texelSize;
        alignas(16) glm::vec4 outerOutlineColor;
        alignas(16) glm::vec4 innerOutlineColor;
        Uint32 ID;
    };

    struct PushConsts {
        float    radius;
        uint32_t flags;
        uint32_t quantaCount;
        uint32_t pad0;
    };
    PushConsts qpc{ 0.05f, 0u, 0u, 0u };

    QuantaSpherePass();
    ~QuantaSpherePass();

    void CreateMaterials() override;
    void CreateImages() override;
    void CreateUniformBuffers() override;
    void CreateDescriptorSetLayout() override;
    void CreateDescriptorPool() override;
    void CreateDescriptorSets() override;
    void CreateGraphicsPipeline() override;
    void Render(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame,
                VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
};
