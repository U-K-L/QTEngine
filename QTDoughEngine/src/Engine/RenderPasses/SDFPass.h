#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
#include "../Renderer/UnigmaRenderingManager.h"
#include "../Camera/UnigmaCamera.h"
#include "ComputePass.h"

class SDFPass : public ComputePass
{
public:
    //This information will be passed into a specialized Voxel compute pass. And a voxel header.
    struct Voxel
	{
		glm::vec4 positionDistance;
		glm::vec4 normalDensity;
	};
    int VOXEL_COUNT = 1; //Set in the creation of the pass.
    int VOXEL_RESOLUTION = 4; //This is the resolution of the 3D texture. n^3
    float SCENE_BOUNDS = 6; //This is the size of the scene bounds. Uniform box. Positioned at the origin of the scene. This is given by the scene description.
    float defaultDistanceMax = 100.0f; //This is the maximum distance for each point in the grid.
    std::vector<std::vector<Voxel>> frameReadbackData; //Generic readback data for the SDF pass.
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
