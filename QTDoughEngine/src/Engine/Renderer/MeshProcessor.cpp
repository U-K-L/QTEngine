#include "MeshProcessor.h"

MeshProcessor* MeshProcessor::instance = nullptr;

MeshProcessor::MeshProcessor()
{

}

MeshProcessor::~MeshProcessor()
{

}

void MeshProcessor::InitMeshProcessor()
{
	vertexSoup = std::vector<Vertex>();
	CreateVertexBuffers();
}

//Comes from various generators that appends to the general soup.
//Clears each frame.
void MeshProcessor::AppendToVerticesSoup(std::vector<Vertex>& incomingVertices)
{
	uint32_t offset = vertexSoup.size();
	uint32_t count = incomingVertices.size();

	for (auto it = incomingVertices.begin(); it != incomingVertices.end(); ++it)
	{
		vertexSoup.push_back(std::move(*it));
	}

	std::tuple<int, int> countsOffsets(count, offset);

	vertexCountsOffsets.push_back(countsOffsets);
}

void MeshProcessor::AppendToVerticesSoup(VkBuffer& incomingVertices, uint32_t count, uint32_t frame, uint32_t srcOffset)
{
	QTDoughApplication* app = QTDoughApplication::instance;
	uint32_t currentVerticesCount = 0;

	std::for_each(vertexCountsOffsets.begin(), vertexCountsOffsets.end(), [&](std::tuple<int,int> countOffset) {
		currentVerticesCount += get<0>(countOffset);
	});

	VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

	VkBufferCopy region{};
	region.srcOffset = srcOffset * sizeof(Vertex);
	region.dstOffset = currentVerticesCount * sizeof(Vertex);
	region.size = count * sizeof(Vertex);

	//Copy buffers on GPU -> GPU.
	vkCmdCopyBuffer(
		commandBuffer,
		incomingVertices,
		vertexSoupBuffer[frame],
		1,
		&region);

	app->EndSingleTimeCommands(commandBuffer);

	uint32_t offset = currentVerticesCount;
	std::tuple<int, int> countsOffsets(count, offset);

	vertexCountsOffsets.push_back(countsOffsets);
}

void MeshProcessor::AppendToVerticesSoup(VkBuffer& incomingVertices, std::vector<uint32_t> counts, uint32_t frame)
{
	QTDoughApplication* app = QTDoughApplication::instance;
	uint32_t preExistingVerticesCount = 0;
	uint32_t srcCounts = 0;

	std::for_each(vertexCountsOffsets.begin(), vertexCountsOffsets.end(), [&](std::tuple<int, int> countOffset) {
		preExistingVerticesCount += get<0>(countOffset);
	});

	std::vector<uint32_t> absOffsets;
	absOffsets.reserve(counts.size());
	std::for_each(counts.begin(), counts.end(), [&](uint32_t count) {
		absOffsets.push_back(preExistingVerticesCount + srcCounts);
		std::tuple<int, int> countsOffsets(count, preExistingVerticesCount + srcCounts);
		vertexCountsOffsets.push_back(countsOffsets);
		srcCounts += count;
	});

	VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();
	uint32_t offsetOffsets = (vertexCountsOffsets.size() - counts.size()) * sizeof(uint32_t);

	VkBufferCopy vertRegion{};
	vertRegion.srcOffset = 0;
	vertRegion.dstOffset = preExistingVerticesCount * sizeof(Vertex);
	vertRegion.size = srcCounts * sizeof(Vertex);

	VkBufferCopy offsetRegion{};
	offsetRegion.srcOffset = 0;
	offsetRegion.dstOffset = offsetOffsets;
	offsetRegion.size = counts.size() * sizeof(uint32_t);

	//Copy buffers on GPU -> GPU.
	vkCmdCopyBuffer(
		commandBuffer,
		incomingVertices,
		vertexSoupBuffer[frame],
		1,
		&vertRegion);


	vkCmdUpdateBuffer(
		commandBuffer,
		vertexOffsetsBuffers[frame],
		offsetRegion.dstOffset,
		offsetRegion.size,
		absOffsets.data()
	);

	/*
	vkCmdCopyBuffer(
		commandBuffer,
		verticesOffsets,
		vertexOffsetsBuffers[frame],
		1,
		&offsetRegion);
	*/

	app->EndSingleTimeCommands(commandBuffer);
}

VkBuffer& MeshProcessor::GetVerticesGPUBuffer(uint32_t currentFrame)
{
	return vertexSoupBuffer[currentFrame];
}

VkBuffer& MeshProcessor::GetVerticesOffsetsGPUBuffer(uint32_t currentFrame)
{
	return vertexOffsetsBuffers[currentFrame];
}

const std::vector<std::tuple<int, int>>& MeshProcessor::GetVerticesCountOffset()
{
	return vertexCountsOffsets;
}

void MeshProcessor::RefreshVertexSoup()
{
	vertexSoup.clear();
	vertexCountsOffsets.clear();
}

void MeshProcessor::UpdateVertexSoup()
{

}

void MeshProcessor::CreateVertexBuffers()
{
	QTDoughApplication* app = QTDoughApplication::instance;

	//Calculate the max size needed.
	glm::ivec3 r = app->WORLD_SDF_RESOLUTION;
	VertexMaxCount = std::max((uint32_t)(r.x * r.x + r.y * r.y + r.z * r.z) / 2u, VertexMaxCount);
	VertexMaxCount -= (VertexMaxCount) % 3;

	VkBufferUsageFlags usage =
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

	VkDeviceSize vertexBufferSize = sizeof(Vertex) * VertexMaxCount;
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

void MeshProcessor::Refresh()
{
	RefreshVertexSoup();
}