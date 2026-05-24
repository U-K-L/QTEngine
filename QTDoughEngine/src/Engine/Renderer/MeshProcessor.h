#pragma once
#include <atomic>
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaRenderingManager.h"

class MeshProcessor
{
	public:
		MeshProcessor();
		~MeshProcessor();
		void MeshProcessor::AppendToVerticesSoup(std::vector<Vertex>& incomingVertices);
		void MeshProcessor::AppendToVerticesSoup(VkCommandBuffer cmd, VkBuffer& incomingVertices, uint32_t count, uint32_t frame);

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

		std::vector<VkBuffer> vertexSoupBuffer;
		std::vector<VkDeviceMemory> vertexSoupMemory;

	private:
		void InitMeshProcessor();
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