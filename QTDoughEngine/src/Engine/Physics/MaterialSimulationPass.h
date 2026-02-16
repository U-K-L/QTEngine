#pragma once
#include <atomic>
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"

#define QUANTA_COUNT 2097152 //Only changes per official build. 

//The particle that emerges from the field.
//Compact, w values may store arbitrary different results.
struct Quanta {
	glm::vec4 position; //The position this quanta is currently in.
	glm::vec4 resonance; //Harmonic, waveform, fourier. Dot(sum(qset(i1), qset(i2)) = resonating.
	glm::ivec4 information; //Hashed ledger, maps to a lookup, a larger ledger.
	glm::vec4 mana; //Potential energy.
};

struct UnigmaField
{
	Unigma3DTexture SignedDistanceField; //3D texture holding the signed distance field. Resolutions changes based on settings.
	glm::ivec3 FieldSize; //invariant holding the size of the field. This can be non-cubic, ie 64x64x16...
	Quanta* Quantas; //The quantas emerging from this field.
};


class MaterialSimulation
{

	public:
		MaterialSimulation();
		~MaterialSimulation();
		static MaterialSimulation* instance;

		void SetInstance(MaterialSimulation* matSim)
		{
			instance = matSim;
		}

		void InitMaterialSim();
		void InitQuanta();
		void InitComputeWorkload(); //eg descriptors, layouts, etc. Calls all below in order.
		void CreateComputeDescriptorSetLayout();
		void CreateDescriptorPool();
		void CreateComputeDescriptorSets();
		void CreateComputePipeline();
		void CreateComputePipelineFromSPV(const std::string& spvName, VkPipeline& outPipeline);
		void CreateSortPipelines();
		void DispatchTileSort(VkCommandBuffer commandBuffer);
		void Simulate(VkCommandBuffer commandBuffer); //Dispatch that happens in main application compute physics.
		void DispatchWaveFunctionCollapse(VkCommandBuffer commandBuffer); //Per-brush collapse after sim.
		void DispatchCollapseFill(VkCommandBuffer commandBuffer); //Per-voxel fill: claim quanta into brush density grid.
		void DispatchBrushFill(VkCommandBuffer commandBuffer, int brushIndex); //Direct per-brush quanta assignment on creation.
		void CopyOutToRead(VkCommandBuffer commandBuffer); //Copies Out buffer to READ buffer after sim.
		void CleanUp();
		void InitQuantaPositions();
		void CreateStorageBuffers();
		void SerializeQuantaBlob(const std::string& path);
		void SerializeQuantaText(const std::string& path);
		void DeserializeQuantaBlob(const std::string& path);
		void ReadBackQuantaFull();
		std::atomic<bool> readbackInProgress{false};
		UnigmaField Field; //Underlying field of everything.

		std::vector<VkBuffer> QuantaStorageBuffers;
		std::vector<VkDeviceMemory> QuantaStorageMemory;

		std::vector<uint32_t> QuantaIds; // used for sorting. 2 of them, one for in/out, the other for READ.
		std::vector<VkBuffer> QuantaIdsBuffer;
		std::vector<VkDeviceMemory> QuantaIdsMemory;

		//Tile approach / binning.

		std::vector<uint32_t> TileCounts; // how many quantas are in each tile. Used for indirect dispatch.
		std::vector<VkBuffer> TileCountsBuffer;
		std::vector<VkDeviceMemory> TileCountsMemory;

		std::vector<uint32_t> TileOffsets; 
		std::vector<VkBuffer> TileOffsetsBuffer;
		std::vector<VkDeviceMemory> TileOffsetsMemory;

		std::vector<uint32_t> TileCursor;
		std::vector<VkBuffer> TileCursorBuffer;
		std::vector<VkDeviceMemory> TileCursorMemory;

		glm::ivec3 TileSize;
		

		struct PushConsts {
			float particleSize;
			int tileGridX;
			int tileGridY;
			int tileGridZ;
			int brushIndex;
			float pad0;
			float pad1;
			float pad2;
		};

	private:
		uint64_t quantaMemorySize;
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;
		VkPipeline pipeline;
		VkPipeline histogramPipeline;
		VkPipeline prefixSumPipeline;
		VkPipeline scatterPipeline;
		VkPipelineLayout pipelineLayout;
		uint32_t currentFrame = 0;

		// Wave Function Collapse — brush access for quanta gather/snap.
		VkBuffer brushesBuffer = VK_NULL_HANDLE;
		VkPipeline collapsePipeline = VK_NULL_HANDLE;
		VkPipeline collapseFillPipeline = VK_NULL_HANDLE;
		VkPipeline brushAssignPipeline = VK_NULL_HANDLE;

		uint32_t dispatchesCount = 0;
};