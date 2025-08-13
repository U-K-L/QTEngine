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

#include "VoxelizerPass.h"
    // Constructor
    AlbedoPass(VoxelizerPass* voxelizer);

    // Destructor
    ~AlbedoPass();

    VoxelizerPass* voxelizer;

    void CreateMaterials() override;
    void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr) override;
    void CreateImages() override;
};