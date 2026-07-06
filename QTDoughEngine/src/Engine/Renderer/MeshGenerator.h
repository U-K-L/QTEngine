#pragma once
#include <atomic>
#include <vector>
#include "UnigmaRenderingStruct.h"

class MeshGenerator
{
	public:
		MeshGenerator();
		virtual ~MeshGenerator();
		virtual void InitMeshGenerator();

		void GeneratePlane();

		virtual void Dispatch(VkCommandBuffer cmd, uint32_t frame);

		virtual void FeedMeshProcessor(uint32_t currentFrame);
		virtual void ConsumeReadback(uint32_t currentFrame);
		virtual void Refresh();

		VkBuffer& GetVertexSoupBuffer(uint32_t frame) { return vertexSoupBuffer[frame]; }

	protected:
		void CreateVertexBuffers();
		virtual void CreateComputeDescriptorSetLayout();
		virtual void CreateDescriptorPool();
		virtual void CreateComputeDescriptorSets();
		virtual void CreateComputePipeline();
		void CreateComputePipelineFromSPV(const std::string& spvName, VkPipeline& outPipeline);

		//TODO: Stdvector RESIZE.
		VkBuffer vertexSoupBuffer[2];
		VkDeviceMemory vertexSoupMemory[2];

		VkBuffer vertexCountBuffers[2];
		VkDeviceMemory vertexCountMemories[2];

		VkBuffer rbVertexCountBuffers[2];
		VkDeviceMemory rbVertexCountMemories[2];

		//For dispatches reading out to. No ping pong.
		VkBuffer processedVertexBuffer[2];
		VkDeviceMemory processedVertexMemory[2];

		uint32_t VertexCapacity = 59049;

	private:
		VkBuffer vertexOffsetsBuffers[2];
		VkDeviceMemory vertexOffsetsMemories[2];
		std::vector<uint32_t> vertexCounts;

		#define MESH_GENERATOR_BINDINGS_COUNT 2
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorSetLayout descriptorSetLayout;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;

		struct PushConsts {
			float pad0;
			float pad1;
			float pad2;
			float pad3;
		};
};
