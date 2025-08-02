#pragma once
#include <string>
#include <cstdint> // For uint32_t
#include "UnigmaMaterial.h"
#include "UnigmaMesh.h"


#include "../../Application/QTDoughApplication.h"

#include "UnigmaRenderingManager.h"

#include "../Core/UnigmaTransform.h"
#include "../Camera/UnigmaCamera.h"
#include "UnigmaRenderingStruct.h"


class UnigmaRenderingObject {
	public:

		struct UniformBufferObject {
			alignas(16) glm::mat4 model;
			alignas(16) glm::mat4 view;
			alignas(16) glm::mat4 proj;
			alignas(16) glm::vec4 baseAlbedo;
			alignas(16) glm::vec4 sideAlbedo;
			alignas(16) glm::vec4 topAlbedo;
			alignas(16) glm::vec2 texelSize;
			alignas(16) glm::vec4 outerOutlineColor;
			alignas(16) glm::vec4 innerOutlineColor;
			Uint32 ID;
		};

		bool isRendering = false;
		UnigmaTransform _transform;
		UnigmaMesh _mesh;
		UnigmaRenderingStruct _renderer;
		UnigmaMaterial _material;
		VkBuffer _vertexBuffer;
		VkDeviceMemory _vertexBufferMemory;
		VkBuffer _indexBuffer;
		VkDeviceMemory _indexBufferMemory;
		std::vector<VkBuffer> _uniformBuffers;
		std::vector<VkDeviceMemory> _uniformBuffersMemory;
		std::vector<void*> _uniformBuffersMapped;
		VkDescriptorSetLayout _descriptorSetLayout;
		VkDescriptorPool _descriptorPool;
		std::vector<VkDescriptorSet> _descriptorSets;
		VkBuffer intArrayBuffer;
		VkDeviceMemory intArrayBufferMemory;

		UnigmaRenderingObject()
			: _mesh(), _material(), _renderer(), _transform()
		{
		}

		UnigmaRenderingObject(UnigmaMesh mesh, UnigmaMaterial mat = UnigmaMaterial())
			: _mesh(mesh), _material(mat), _renderer(), _transform()
		{
		}

		// Copy assignment operator
		UnigmaRenderingObject& operator=(const UnigmaRenderingStructCopyableAttributes& other) {
			this->_transform = other._transform;
			this->_renderer = other._renderer;
			return *this;
		}

		void Initialization(UnigmaMaterial material);
		void CreateVertexBuffer(QTDoughApplication& app);
		void CreateIndexBuffer(QTDoughApplication& app);
		void Render(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame);
		void RenderPass(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSet);
		void RenderObjectToUnigma(QTDoughApplication& app, RenderObject& rObj, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& cam);
		void LoadModel(UnigmaMesh& mesh);
		void Cleanup(QTDoughApplication& app);
		void LoadBlenderMeshData(RenderObject& rObj);
		void UpdateUniformBuffer(QTDoughApplication& app, uint32_t currentImage, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& camera, void* uniformMem);
		void UnigmaRenderingObject::UpdateUniformBuffer(QTDoughApplication& app, uint32_t currentImage, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& camera, void* uniformMem, UniformBufferObject ubo);
		void CreateDescriptorSets(QTDoughApplication& app);
		void CreateDescriptorPool(QTDoughApplication& app);
		void CreateUniformBuffers(QTDoughApplication& app);
		void CreateDescriptorSetLayout(QTDoughApplication& app);
		void CreateGraphicsPipeline(QTDoughApplication& app);
		void RenderBrush(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSet, VkBuffer& vertexBuffer, uint32_t readBackVertexCount, VkBuffer indirectDrawBuffer);
	private:
};
