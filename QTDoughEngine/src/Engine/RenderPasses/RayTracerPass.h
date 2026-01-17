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
    void CreateComputePipeline();
    void Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame);
    void CreateShaderStorageBuffers();
    void DebugCompute(uint32_t currentFrame);
    void CreateMaterials();
    void UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain);
    void AddObjects(UnigmaRenderingObject* unigmaRenderingObjects);
    void CreateTriangleSoup();

    float passWidth = 720;
    float passHeight = 720;
    std::string PassName = "RayTracerPass";
    std::vector<std::string> PassNames;

    //RT function pointers
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR_fn = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_fn = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_fn = nullptr;

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

    //TLAS handle
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;

    //Output storage image view
    VkImageView rtOutputView = VK_NULL_HANDLE;


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
    VkPipeline computePipeline;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> computeDescriptorSets;
    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VkDeviceMemory> _uniformBuffersMemory;
    std::vector<void*> _uniformBuffersMapped;
    VkPipelineLayout computePipelineLayout;
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
