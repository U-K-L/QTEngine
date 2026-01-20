#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
#include "../Renderer/UnigmaRenderingManager.h"
#include "../Camera/UnigmaCamera.h"

class RayTracerPass
{
public:
    // Constructor
    RayTracerPass();

    // Destructor
    ~RayTracerPass();

    void CreateComputeDescriptorSets();
    void CreateComputeDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateComputePipeline();
    void Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame);
    void CreateShaderStorageBuffers();
    void DebugCompute(uint32_t currentFrame);
    void CreateMaterials();
    void UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain);
    void AddObjects(UnigmaRenderingObject* unigmaRenderingObjects);
    void CreateTriangleSoup();
    void CreateSBT();
    void CreateImages();
    void CreateUniformBuffers();

    float passWidth = 720;
    float passHeight = 720;
    std::string PassName = "RayTracerPass";
    std::vector<std::string> PassNames;

    //RT function pointers
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR_fn = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_fn = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_fn = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress_fn = nullptr;
    PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2_fn = nullptr;

    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR_fn = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_fn = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR_fn = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR_fn = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_fn = nullptr;

    //RT properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    // RT pipeline objects
    VkPipeline rtPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> rtDescriptorSets;

    //SBT
    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;

    VkStridedDeviceAddressRegionKHR raygenRegion, missRegion, hitRegion, callableRegion;

    // Acceleration Structures
    struct RtASPerFrame
    {
        // BLAS
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        VkBuffer blasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory blasMemory = VK_NULL_HANDLE;

        // TLAS
        VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
        VkBuffer tlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory tlasMemory = VK_NULL_HANDLE;

        VkBuffer scratchBuffer = VK_NULL_HANDLE;
        VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
        VkDeviceSize scratchSize = 0;

        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceMemory = VK_NULL_HANDLE;

        VkDeviceAddress blasAddr = 0;
        VkDeviceAddress tlasAddr = 0;
    };
    std::vector<RtASPerFrame> rtAS;


    VkDeviceAddress GetBufferAddress(VkBuffer buffer);
    VkDeviceAddress GetASAddress(VkAccelerationStructureKHR as);

    void BuildBLAS_FromTriangleSoup(VkCommandBuffer cmd, uint32_t frame, VkBuffer vertexBuffer, VkDeviceSize vertexOffset, uint32_t vertexCount, VkDeviceSize vertexStride);

    void BuildTLAS_SingleInstance(
        VkCommandBuffer cmd,
        uint32_t frame,
        const VkTransformMatrixKHR& xform = { {
            {1,0,0,0},
            {0,1,0,0},
            {0,0,1,0}
        } });

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec4 texelSize;
        alignas(16) float isOrtho;
    };


    //Output storage image view
    VkImageView rtOutputView = VK_NULL_HANDLE;
    VkDeviceSize asScratchSize = 0;

    std::vector<VkBuffer> shaderStorageBuffers;
    std::vector<VkDeviceMemory> shaderStorageBuffersMemory;

    std::vector<UnigmaRenderingObject*> renderingObjects;
    UnigmaMaterial material;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkDeviceMemory> imagesMemory;
    std::vector<VkImageView> imagesViews;
    VkDescriptorPool descriptorPool;
    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VkDeviceMemory> _uniformBuffersMemory;
    std::vector<void*> _uniformBuffersMapped;
    VkFramebuffer offscreenFramebuffer;
    VkBuffer intArrayBuffer;
    VkDeviceMemory intArrayBufferMemory;
    std::vector<VkCommandBuffer> _commandBuffers;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    VkCommandPool _commandPool;
    std::vector<VkBuffer> readbackBuffers;
    std::vector<VkDeviceMemory> readbackBufferMemories;
    std::vector<UnigmaRenderingObject*> GameObjects;

};
