#include "MeshGenerator.h"
#include "MeshProcessor.h"
#include "../Physics/MaterialSimulationPass.h"

MeshGenerator* MeshGenerator::instance = nullptr;

MeshGenerator::MeshGenerator()
{

}

MeshGenerator::~MeshGenerator()
{

}

void MeshGenerator::InitMeshGenerator()
{
	CreateVertexBuffers();
}

void MeshGenerator::GeneratePlane()
{
	QTDoughApplication* app = QTDoughApplication::instance;
	MeshProcessor *meshProcessor = MeshProcessor::instance;
	uint32_t currentFrame = app->currentFrame;

	VkBuffer quadBuf = VK_NULL_HANDLE;
	VkDeviceMemory quadMem = VK_NULL_HANDLE;
	VkDeviceSize quadBytes = sizeof(Vertex) * 6;
	void* quadMapped = nullptr;

	app->CreateBuffer(quadBytes,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		quadBuf, quadMem);

	vkMapMemory(app->_logicalDevice, quadMem, 0, quadBytes, 0, &quadMapped);

	glm::vec3 c(0.0f);
	if (MaterialSimulation::instance && MaterialSimulation::instance->brushMatricies.size() > 22)
		c = glm::vec3(MaterialSimulation::instance->brushMatricies[22].bCentroid);

	const glm::vec4 nUp   = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	const glm::vec4 white = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

	Vertex verts[6] = {};
	verts[0].pos = glm::vec4(c.x - 0.5f, c.y - 0.5f, c.z, 0.0f);
	verts[1].pos = glm::vec4(c.x + 0.5f, c.y - 0.5f, c.z, 0.0f);
	verts[2].pos = glm::vec4(c.x - 0.5f, c.y + 0.5f, c.z, 0.0f);
	verts[3].pos = glm::vec4(c.x + 0.5f, c.y - 0.5f, c.z, 0.0f);
	verts[4].pos = glm::vec4(c.x + 0.5f, c.y + 0.5f, c.z, 0.0f);
	verts[5].pos = glm::vec4(c.x - 0.5f, c.y + 0.5f, c.z, 0.0f);

	verts[0].texCoord = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	verts[1].texCoord = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	verts[2].texCoord = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	verts[3].texCoord = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	verts[4].texCoord = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
	verts[5].texCoord = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

	for (int i = 0; i < 6; ++i)
	{
		verts[i].normal = nUp;
		verts[i].color  = white;
	}

	memcpy(quadMapped, verts, sizeof(verts));

	std::vector<uint32_t> vertexCounts = { 6 };

	meshProcessor->AppendToVerticesSoup(quadBuf, vertexCounts, currentFrame);
}

void MeshGenerator::CreateVertexBuffers()
{
	QTDoughApplication* app = QTDoughApplication::instance;

	//Calculate the max size needed.
	glm::ivec3 r = app->WORLD_SDF_RESOLUTION;

	VkBufferUsageFlags usage =
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

	VkDeviceSize vertexBufferSize = sizeof(Vertex) * VertexCapacity;
	VkDeviceSize vertexOffsetMaxSize = sizeof(uint32_t) * NUM_OBJECTS;

	for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++)
	{
		app->CreateBuffer(
			vertexBufferSize,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexSoupBuffer[i],
			vertexSoupMemory[i]);

		app->CreateBuffer(
			vertexOffsetMaxSize,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexOffsetsBuffers[i],
			vertexOffsetsMemories[i]);
	}
}
