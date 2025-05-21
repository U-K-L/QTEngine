#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
#include "../Renderer/UnigmaRenderingManager.h"
#include "../Camera/UnigmaCamera.h"
#include "ComputePass.h"

class VoxelizerPass : public ComputePass
{
public:

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec2 texelSize;
    };

    static void SetInstance(VoxelizerPass* voxelizer)
    {
        instance = voxelizer;
    }
    static VoxelizerPass* instance;
    //This information will be passed into a specialized Voxel compute pass. And a voxel header.
    //We're trying to keep the grid at 1 GB VRAM maximum. This gives us around 512 bytes, or 32 float4s.
    
    //This voxel is for L1, in the future we also want a Voxel struct with more information but higher MIP.
    struct Voxel {
        glm::vec4 normalDistance; // packed half4: 4 × 16-bit = 8 bytes
    };
    int VOXEL_COUNTL1 = 1; //Set in the creation of the pass.
    int VOXEL_RESOLUTIONL1 = 256; //This is the resolution of the 3D texture. n^3
    int VOXEL_RESOLUTIONL2 = 128;
    int VOXEL_RESOLUTIONL3 = 64;
    float SCENE_BOUNDSL1 = 8; //This is the size of the scene bounds. Uniform box. Positioned at the origin of the scene. This is given by the scene description.
    float SCENE_BOUNDSL2 = 16;
    float SCENE_BOUNDSL3 = 32;
    int VOXEL_COUNTL2 = 1;
    int VOXEL_COUNTL3 = 1;


    float defaultDistanceMax = 100.0f; //This is the maximum distance for each point in the grid.
    std::vector<std::vector<Voxel>> frameReadbackData; //Generic readback data for the SDF pass.
    std::vector<Voxel> voxelsL1; 
    std::vector<Voxel> voxelsL2; 
    std::vector<Voxel> voxelsL3;

    int UpdateOnce = 0;

    struct Triangle {
        glm::vec3 a, b, c;
        glm::vec3 normal;
    };
    std::vector<Triangle> triangleSoup;

    std::vector<VkBuffer> voxelL1StorageBuffers;
    std::vector<VkDeviceMemory> voxelL1StorageBuffersMemory;

    std::vector<VkBuffer> voxelL2StorageBuffers;
    std::vector<VkDeviceMemory> voxelL2StorageBuffersMemory;

    std::vector<VkBuffer> voxelL3StorageBuffers;
    std::vector<VkDeviceMemory> voxelL3StorageBuffersMemory;

    // Constructor
    VoxelizerPass();

    // Destructor
    ~VoxelizerPass();

    void CreateComputeDescriptorSets() override;
    void CreateComputeDescriptorSetLayout() override;
    void CreateComputePipeline() override;
    void Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
    void CreateShaderStorageBuffers() override;
    void DebugCompute(uint32_t currentFrame) override;
    void CreateMaterials() override;
    void UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) override;
    void IsOccupiedByVoxel();
    void BakeSDFFromTriangles();
    float DistanceToTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);


    std::vector<Triangle> ExtractTrianglesFromMeshFromTriplets(const std::vector<ComputeVertex>& vertices, const std::vector<glm::uvec3>& triangleIndices);
};
