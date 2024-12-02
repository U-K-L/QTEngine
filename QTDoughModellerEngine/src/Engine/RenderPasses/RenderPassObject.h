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

        std::vector<UnigmaRenderingObject*> renderingObjects;
        UnigmaMaterial material;
        std::string PassName = "DefaultPass";
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
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

        virtual void CreateUniformBuffers();
        virtual void CreateDescriptorPool();
        virtual void CreateDescriptorSets();
        virtual void CreateGraphicsPipeline();
        virtual void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr, UnigmaCameraStruct* CameraMain = nullptr);
        virtual void CreateImages();
        virtual void CreateDescriptorSetLayout();
        virtual void CreateMaterials();
        virtual void UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame);
        virtual void CleanupPipeline();
        virtual void RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain);
        virtual void UpdateUniformBufferObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain);
};