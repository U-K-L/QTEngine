#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
#include "../Renderer/UnigmaRenderingManager.h"
#include "../Camera/UnigmaCamera.h"
class RenderPassObject
{
	public:
        // Constructor
        RenderPassObject();

        // Destructor
        ~RenderPassObject();

        struct UniformBufferObject {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
            alignas(16) glm::vec2 texelSize;
        };

        std::vector<UnigmaRenderingObject*> renderingObjects;
        UnigmaMaterial material;
        std::string PassName = "DefaultPass";
        std::vector<std::string> PassNames; //too lazy to refactor forgive me.
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        std::vector<VkImage> images;
        std::vector<VkDeviceMemory> imagesMemory;
        std::vector<VkImageView> imagesViews;
        VkPipeline graphicsPipeline;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;
        std::vector<VkBuffer> _uniformBuffers;
        std::vector<VkDeviceMemory> _uniformBuffersMemory;
        std::vector<void*> _uniformBuffersMapped;
        VkPipelineLayout pipelineLayout;
        VkFramebuffer offscreenFramebuffer;
        VkBuffer intArrayBuffer;
        VkDeviceMemory intArrayBufferMemory;

        std::vector<UnigmaRenderingObject*> GameObjects;

        float passWidth = 720;
        float passHeight = 720;

        virtual void CreateUniformBuffers();
        virtual void CreateDescriptorPool();
        virtual void CreateDescriptorSets();
        virtual void CreateGraphicsPipeline();
        virtual void AddObjects(UnigmaRenderingObject* unigmaRenderingObjects);
        virtual void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr);
        virtual void CreateImages();
        virtual void CreateDescriptorSetLayout();
        virtual void CreateMaterials();
        virtual void UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame);
        virtual void CleanupPipeline();
        virtual void RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, UnigmaCameraStruct* CameraMain);
        virtual void UpdateUniformBufferObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain);
        virtual void RenderPerObject(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain);
        virtual void RenderPerObject(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, std::vector<VkImageView*> targetImages, UnigmaCameraStruct* CameraMain);
};