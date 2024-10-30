#include "RenderPassObject.h"

RenderPassObject::~RenderPassObject() {
}

RenderPassObject::RenderPassObject() {}

void RenderPassObject::CreateDescriptorPool() {}

void RenderPassObject::CreateDescriptorSets() {}

void RenderPassObject::CreateGraphicsPipeline() {}

void RenderPassObject::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex) {}

void RenderPassObject::CreateImages() {}

void RenderPassObject::CreateDescriptorSetLayout() {}