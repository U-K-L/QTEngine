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
        virtual void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkImageView* targetImage = nullptr);
        virtual void CreateImages();
        virtual void CreateDescriptorSetLayout();
        virtual void CreateMaterials();
};