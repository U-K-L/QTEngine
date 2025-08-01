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
        alignas(16) glm::vec4 texelSize;
        alignas(16) float isOrtho;
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
        uint32_t distance;
        float id;
        uint32_t brushId;
        uint32_t pad3;
        glm::vec4 normalDistance; // packed half4: 4 � 16-bit = 8 bytes
    };

    //Struct of brushes. Most brushes are meshes with a model matrix. However, analytical brushes can be provided as well.
    //Brushes are basically tied to gameObjects.
    //256 bytes is the limit.
    struct Brush
    {
        uint32_t type; 
        uint32_t vertexCount;
        uint32_t vertexOffset;
        uint32_t textureID;
        uint32_t textureID2;
        uint32_t resolution;
        glm::mat4 model;
        glm::mat4 invModel;
        glm::vec4 aabbmin;
        glm::vec4 aabbmax;
        glm::vec4 center;
        uint32_t isDirty; //determines if needs an update. 0 = no update needed, 1 = update needed, 2 = not active.
        float stiffness;
        float blend;
        uint32_t opcode;
        uint32_t id;
        float smoothness;
    };

    struct Tile
    {
        uint32_t brushCount;
        uint32_t brushOffset;
    };



    struct PushConsts {
        float lod;
        uint32_t triangleCount;
    };

    int VOXEL_COUNTL1 = 1; //Set in the creation of the pass.
    int WORLD_SDF_RESOLUTION = 512;
    int VOXEL_RESOLUTIONL1 = 256; //This is the resolution of the 3D texture. n^3
    int VOXEL_RESOLUTIONL2 = 128;
    int VOXEL_RESOLUTIONL3 = 64;
    float SCENE_BOUNDSL1 = 16; //This is the size of the scene bounds. Uniform box. Positioned at the origin of the scene. This is given by the scene description.
    float SCENE_BOUNDSL2 = 16;
    float SCENE_BOUNDSL3 = 32;
    int VOXEL_COUNTL2 = 1;
    int VOXEL_COUNTL3 = 1;

    int DeformResolution = 1; //number of voxels per deform thread.

    uint32_t TILE_SIZE = 8;          // voxels per edge
    uint32_t TILE_MAX_BRUSHES = 32;     // cap of brushes per tile
    uint32_t TILE_COUNTL1 = 0;


    uint32_t dispatchCount = 0;
    uint32_t IDDispatchIteration = 0;
    uint32_t requiredIterations = 60;

    bool flagSDFBaker = false; //This is a flag to bake the SDF from triangles.


    float defaultDistanceMax = 100.0f; //This is the maximum distance for each point in the grid.
    std::vector<std::vector<Voxel>> frameReadbackData; //Generic readback data for the SDF pass.
    std::vector<Voxel> voxelsL1; 
    std::vector<Voxel> voxelsL2; 
    std::vector<Voxel> voxelsL3;

    //This is the list of brushes. Brushes are basically gameObjects with a model matrix.
    //Some fields only updated once per generation which can take multiple frames. However, the vector itself updates every frame.
    std::vector<Brush> brushes; 
    VkBuffer brushesStorageBuffers;
    VkDeviceMemory brushesStorageMemory;

    //This is the list of tiles. Tiles are basically a collection of brushes.
    std::vector<uint32_t> BrushesIndices;
    VkBuffer brushIndicesStorageBuffers;
    VkDeviceMemory brushIndicesStorageMemory;

    //Tile brush counts.
    std::vector<uint32_t> TilesBrushCounts;
    VkBuffer tileBrushCountStorageBuffers;
    VkDeviceMemory tileBrushCountStorageMemory;

    //GlobalCounter
    std::vector<uint32_t> GlobalIDCounter;
    VkBuffer globalIDCounterStorageBuffers;
    VkDeviceMemory globalIDCounterStorageMemory;

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

    VkPipeline voxelizeComputePipeline;
    VkPipelineLayout voxelizeComputePipelineLayout;
    VkPipeline tileGenerationComputePipeline;
    VkPipelineLayout tileGenerationComputePipelineLayout;

    VkBuffer  voxL1PingPong[2]{};
    VkBuffer  voxL2PingPong[2]{};
    VkBuffer  voxL3PingPong[2]{};
    VkDeviceMemory voxL1PingPongMem[2]{};
    VkDeviceMemory voxL2PingPongMem[2]{};
    VkDeviceMemory voxL3PingPongMem[2]{};

    VkDescriptorSet sweepSets[2]{};
    VkDescriptorSet currentSdfSet = VK_NULL_HANDLE;

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
    void UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) override;
    void IsOccupiedByVoxel();
    void BakeSDFFromTriangles();
    float DistanceToTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);
    void DispatchLOD(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel, bool pingFlag = false);
    void CreateComputePipelineName(std::string shaderPass, VkPipeline& rcomputePipeline, VkPipelineLayout& rcomputePipelineLayout);
    void DispatchTile(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel);
    void CreateImages() override;
    void Create3DTextures();
    void CreateBrushes();
    void DispatchBrushCreation(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel);
    void UpdateBrushesGPU(VkCommandBuffer commandBuffer);
    void BindVoxelBuffers(uint32_t curFrame, uint32_t prevFrame, bool pingFlag);
    void CreateSweepDescriptorSets();
    void PerformEikonalSweeps(VkCommandBuffer cmd, uint32_t curFrame);
    void CreateDescriptorPool() override;
    void DispatchBrushDeformation(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t brushID);
    void UpdateBrushesTextureIds(VkCommandBuffer commandBuffer);
    void CleanUpGPU(VkCommandBuffer commandBuffer);
    void DispatchBrushGeneration(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lod, uint32_t brushID);
    void VoxelizerPass::GetMeshFromGPU(uint32_t vertexCount);

    std::vector<Triangle> ExtractTrianglesFromMeshFromTriplets(const std::vector<ComputeVertex>& vertices, const std::vector<glm::uvec3>& triangleIndices);

    //Some fluid particles test. Move this to its own pass later on.
    int PARTICLE_COUNT = 262144;

    //128 particle data fits in a single modern GPU data lane... try to get it to 64, but always keep it multiples of 32 since some lanes are 192.
    struct Particle {
        glm::vec4 position;
        glm::vec4 velocity;
        glm::vec4 force;
        glm::vec4 tbd;
        glm::vec4 tbd2;
        glm::vec4 tbd3;
        glm::vec4 tbd4;
    };

    std::vector<Particle> particles;

    std::vector<VkBuffer> particlesStorageBuffers;
    std::vector<VkDeviceMemory> particlesStorageMemory;

    int CAGE_RESOLUTION = 26; //Resolution of the cage for deformation.
    int CONTROL_PARTICLE_COUNT = 4096 * CAGE_RESOLUTION; //The total amount of particles is the amount of deformable objects multipled by the cage resolution.

    struct ControlParticle {
        glm::vec4 position;
    };

    std::vector<Particle> controlParticles;

    std::vector<VkBuffer> controlParticlesStorageBuffers;
    std::vector<VkDeviceMemory> controlParticlesStorageMemory;

    std::vector<Vertex> meshVertices;
    std::vector<ComputeVertex> meshingVertexSoup;
    std::vector<glm::uvec3> meshingTriangleIndices;
    VkBuffer meshingVertexBuffer;
    VkBuffer vertexBufferReadbackBuffer;
    VkDeviceMemory vertexBufferReadbackMemory;
    VkDeviceMemory meshingVertexBufferMemory;
    VkBuffer meshingIndexBuffer;
    VkDeviceMemory meshingIndexBufferMemory;

    VkBuffer       meshingStagingBuffer;
    VkDeviceMemory meshingStagingBufferMemory;
    VkFence        meshingReadbackFence;

    VkImage wu_image;
    VkDeviceMemory wu_imageMemory;
    VkImageView wu_imageView;
    VkSampler wu_sampler;

    bool VolumeTexturesCreated = false;
    int mipsCount = 5;
    uint32_t readBackVertexCount = 0;
};
