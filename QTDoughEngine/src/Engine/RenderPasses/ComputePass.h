#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
#include "../Renderer/UnigmaRenderingManager.h"
#include "../Camera/UnigmaCamera.h"
class ComputePass
{
public:
    // Constructor
    ComputePass();

    // Destructor
    ~ComputePass();

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec2 texelSize;
    };

    int PARTICLE_COUNT = 10000000;
    struct Particle {
        glm::vec2 position;
        glm::vec2 velocity;
        glm::vec4 color;
    };
    std::vector<VkBuffer> shaderStorageBuffers;
    std::vector<VkDeviceMemory> shaderStorageBuffersMemory;

    std::vector<UnigmaRenderingObject*> renderingObjects;
    UnigmaMaterial material;
    std::string PassName = "DefaultComputePass";
    std::vector<std::string> PassNames;
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

    virtual void CreateUniformBuffers();
    virtual void CreateComputeDescriptorSetLayout();
    virtual void CreateComputeDescriptorSets();
    virtual void CreateComputePipeline();
    virtual void AddObjects(UnigmaRenderingObject* unigmaRenderingObjects);
    //virtual void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr);
    virtual void Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame);
    virtual void CreateImages();
    virtual void CreateMaterials();
    virtual void CreateShaderStorageBuffers();
    virtual void CreateComputeCommandBuffers();
    virtual void CreateDescriptorPool();
    //virtual void UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame);
    virtual void UpdateUniformBufferObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain);
    //virtual void CleanupPipeline();
    virtual void DebugCompute(uint32_t currentFrame);
};