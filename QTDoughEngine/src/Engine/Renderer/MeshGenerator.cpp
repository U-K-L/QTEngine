#include "MeshGenerator.h"
#include "MeshProcessor.h"
#include "../Physics/MaterialSimulationPass.h"
#include "../../Application/QTDoughApplication.h"

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

void MeshGenerator::Refresh()
{
	vertexCounts.clear();
}

void MeshGenerator::Dispatch(VkCommandBuffer cmd, uint32_t frame)
{
	GeneratePlane();
}

void MeshGenerator::ConsumeReadback(uint32_t currentFrame)
{
	//This is for indirect rendering.
}

void MeshGenerator::FeedMeshProcessor(uint32_t currentFrame)
{
	///We have our vertex soup, and we feed it to the larger vertex soup to be processed.
	//We'll wait for all the other guys like us to finish as well!
	MeshProcessor *meshProcessor = MeshProcessor::instance;
	meshProcessor->AppendToVerticesSoup(vertexSoupBuffer[currentFrame], vertexCounts, currentFrame);
}

void MeshGenerator::GeneratePlane()
{
	QTDoughApplication* app = QTDoughApplication::instance;
	MeshProcessor *meshProcessor = MeshProcessor::instance;
	uint32_t currentFrame = app->currentFrame;

	uint32_t numOfVertices = 6;
	vertexCounts.push_back(numOfVertices);

	VkBuffer quadBuf = VK_NULL_HANDLE;
	VkDeviceMemory quadMem = VK_NULL_HANDLE;
	VkDeviceSize quadBytes = sizeof(Vertex) * numOfVertices;
	void* quadMapped = nullptr;

	app->CreateBuffer(quadBytes,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		quadBuf, quadMem);

	vkMapMemory(app->_logicalDevice, quadMem, 0, quadBytes, 0, &quadMapped);

	glm::vec3 c(0.0f, 0.0f, 2.0f);

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

	VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

	VkBufferCopy vertRegion{};
	vertRegion.srcOffset = 0;
	vertRegion.dstOffset = 0; //This will change because multiple copies can occur in one generator???
	vertRegion.size = quadBytes;

	vkCmdCopyBuffer(
	commandBuffer,
	quadBuf,
	vertexSoupBuffer[currentFrame],
	1,
	&vertRegion);

	app->EndSingleTimeCommands(commandBuffer);
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

		app->CreateBuffer(
			sizeof(uint32_t),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexCountBuffers[i],
			vertexCountMemories[i]);

		app->CreateBuffer(
			sizeof(uint32_t),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			rbVertexCountBuffers[i],
			rbVertexCountMemories[i]);
	}
}
