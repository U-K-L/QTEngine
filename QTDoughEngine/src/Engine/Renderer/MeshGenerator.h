#pragma once
#include <atomic>
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaRenderingManager.h"

class MeshGenerator
{
	public:
		MeshGenerator();
		~MeshGenerator();
		void InitMeshGenerator();

		void GeneratePlane();

		static MeshGenerator* instance;
		void SetInstance(MeshGenerator* meshGen)
		{
			instance = meshGen;
		}


	private:
		void CreateVertexBuffers();
		VkBuffer vertexSoupBuffer[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];
		VkDeviceMemory vertexSoupMemory[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];

		VkBuffer vertexOffsetsBuffers[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];
		VkDeviceMemory vertexOffsetsMemories[QTDoughApplication::MAX_FRAMES_IN_FLIGHT];

		uint32_t VertexCapacity = 59049;

};