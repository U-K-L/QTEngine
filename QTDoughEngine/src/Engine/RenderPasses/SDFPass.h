#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
#include "../Renderer/UnigmaRenderingManager.h"
#include "../Camera/UnigmaCamera.h"
#include "ComputePass.h"

class SDFPass : public ComputePass
{
public:
    struct Voxel
	{
		glm::vec4 positionDistance;
		glm::vec4 normalDensity;
	};
    int VOXEL_COUNT = 64;
    std::vector<std::vector<Voxel>> frameReadbackData;
    // Constructor
    SDFPass();

    // Destructor
    ~SDFPass();

    void CreateComputeDescriptorSets() override;
    void CreateComputeDescriptorSetLayout() override;
    void CreateComputePipeline() override;
    void Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
    void CreateShaderStorageBuffers() override;
    void DebugCompute(uint32_t currentFrame) override;
    void CreateMaterials() override;
};
