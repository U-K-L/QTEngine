#pragma once
#include <atomic>
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaRenderingManager.h"
#define MAX_MESH_INSTANCES 16384

class MeshProcessor
{
	public:
		MeshProcessor();
		~MeshProcessor();
		void InitMeshProcessor();
		void MeshProcessor::AppendToVerticesSoup(std::vector<Vertex>& incomingVertices);
		void MeshProcessor::AppendToVerticesSoup(VkBuffer& incomingVertices, uint32_t count, uint32_t frame, uint32_t srcOffset);
		void MeshProcessor::AppendToVerticesSoup(VkBuffer& incomingVertices, std::vector<uint32_t> counts, uint32_t frame);
		void MeshProcessor::Refresh();
		VkBuffer& GetVerticesGPUBuffer(uint32_t currentFrame);
		VkBuffer& GetVerticesOffsetsGPUBuffer(uint32_t currentFrame);
		const std::vector<std::tuple<int, int>>& GetVerticesCountOffset();

		static MeshProcessor* instance;

		void SetInstance(MeshProcessor* meshProc)
		{
			instance = meshProc;
		}

		//Preprocessed vertex soup, givene to us by multiple generators such as:
		//Dual Contour, CPU authored meshes, foilage generator, particle generator, best fit plane etc.
		std::vector<Vertex> vertexSoup;

		//The vertices after being processed. Multiple processors may happen depending on effects.
		//This is also the list that gets culled.
		std::vector<Vertex> procesedVertices;

		//Post culled sorted vertices by Object ID.
		std::vector<Vertex> sortedVertices;

		//These are the post processed sorted vertices that are feed to the GPU's RTA.
		//The engine only works on hardware RTA capable.
		std::vector<std::vector<Vertex>> bLasVertices;

		std::vector<std::tuple<int, int>> vertexCountsOffsets;

		std::vector<glm::uvec3> meshingTriangleIndices;


		//Vulkan memory things.
		uint32_t VertexMaxCount = 524286; //2^19 - 2. Max number of vertices for GPU side triangle soup. Recalc depending on settings.

		VkBuffer vertexSoupBuffer[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];
		VkDeviceMemory vertexSoupMemory[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];

		VkBuffer vertexOffsetsBuffers[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];
		VkDeviceMemory vertexOffsetsMemories[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];

	private:
		void UpdateVertexSoup();
		void CreateVertexBuffers();
		void CreateBuffers();
		void CreateDescriptorSets();
		void CreateDescriptorLayouts();
		void InitializePipelines();
		void GPUVerticesReadback();
		void Dispatch();
		void RefreshVertexSoup();
		
};