#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"
class RenderPassObject
{
	public:
        // Constructor
        RenderPassObject();

        // Destructor
        ~RenderPassObject();

        UnigmaMaterial material;
        std::string PassName = "DefaultPass";
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
        VkPipeline graphicsPipeline;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;
        VkPipelineLayout pipelineLayout;
        VkFramebuffer offscreenFramebuffer;
        VkBuffer intArrayBuffer;
        VkDeviceMemory intArrayBufferMemory;


        virtual void CreateDescriptorPool();
        virtual void CreateDescriptorSets();
        virtual void CreateGraphicsPipeline();
        virtual void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage = nullptr);
        virtual void CreateImages();
        virtual void CreateDescriptorSetLayout();
        virtual void CreateMaterials();
        virtual void UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame);
        virtual void CleanupPipeline();
};