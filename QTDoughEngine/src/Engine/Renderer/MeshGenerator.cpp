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
	CreateComputeDescriptorSetLayout();
	CreateDescriptorPool();
	CreateComputeDescriptorSets();
	//CreateComputePipeline();
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
			vertexBufferSize,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			processedVertexBuffer[i],
			processedVertexMemory[i]);

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

void MeshGenerator::CreateComputeDescriptorSets()
{
	// Binding 0: Vertices In (read)
	// Binding 1: Vertices Out (write)

	VkDevice device = QTDoughApplication::instance->_logicalDevice;
	uint32_t buffersInFlight = QTDoughApplication::MAX_FRAMES_IN_FLIGHT;

	std::vector<VkDescriptorSetLayout> layouts(buffersInFlight, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = buffersInFlight;
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(buffersInFlight);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("MeshGeenerator: failed to allocate descriptor sets!");
	}

	//Will eventually bind quanta and the grid, leave alone for now.
	//uint64_t quantaBufferSize = sizeof(Quanta) * QUANTA_COUNT;
	uint64_t vertexBufferSize = sizeof(Vertex) * VertexCapacity;

	for (uint32_t i = 0; i < buffersInFlight; i++)
	{
		VkDescriptorBufferInfo verticesInfo{};
		verticesInfo.buffer = vertexSoupBuffer[i];
		verticesInfo.offset = 0;
		verticesInfo.range = vertexBufferSize;

		VkDescriptorBufferInfo processedVerticesInfo{};
		processedVerticesInfo.buffer = processedVertexBuffer[i];
		processedVerticesInfo.offset = 0;
		processedVerticesInfo.range = vertexBufferSize;


		std::array<VkWriteDescriptorSet, MESH_GENERATOR_BINDINGS_COUNT> writes{};
		VkDescriptorBufferInfo* bufferInfos[] = {
			&verticesInfo, &processedVerticesInfo
		};

		for (uint32_t b = 0; b < writes.size(); b++)
		{
			writes[b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[b].dstSet = descriptorSets[i];
			writes[b].dstBinding = b;
			writes[b].dstArrayElement = 0;
			writes[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[b].descriptorCount = 1;
			writes[b].pBufferInfo = bufferInfos[b];
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}
}

void MeshGenerator::CreateComputeDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings{};

	bindings.resize(MESH_GENERATOR_BINDINGS_COUNT);

	for (uint32_t i = 0; i < bindings.size(); i++)
	{
		bindings[i].binding = i;
		bindings[i].descriptorCount = 1;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[i].pImmutableSamplers = nullptr;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(QTDoughApplication::instance->_logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("MeshGenerator: failed to create descriptor set layout!");
	}
}

void MeshGenerator::CreateDescriptorPool()
{
	VkDevice device = QTDoughApplication::instance->_logicalDevice;
	uint32_t buffersInFlight = QTDoughApplication::MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = MESH_GENERATOR_BINDINGS_COUNT * buffersInFlight;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = buffersInFlight;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("MaterialSimulation: failed to create descriptor pool!");
	}
}


void MeshGenerator::CreateComputePipeline()
{
	QTDoughApplication* app = QTDoughApplication::instance;

	//TODO create the plane shader here.
	auto shaderCode = readFile("src/shaders/materialsim.spv");
	VkShaderModule shaderModule = app->CreateShaderModule(shaderCode);

	VkPipelineShaderStageCreateInfo stageInfo{};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shaderModule;
	stageInfo.pName = "main";

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConsts);

	std::array<VkDescriptorSetLayout, 2> layouts = {
		app->globalDescriptorSetLayout,
		descriptorSetLayout
	};

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutInfo.pSetLayouts = layouts.data();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(app->_logicalDevice, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("MeshGenerator: failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.stage = stageInfo;

	if (vkCreateComputePipelines(app->_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("MeshGenerator: failed to create compute pipeline!");
	}

	vkDestroyShaderModule(app->_logicalDevice, shaderModule, nullptr);
	//CreateComputePipelineFromSPV("matsim_collapse", collapsePipeline);
}

void MeshGenerator::CreateComputePipelineFromSPV(const std::string& spvName, VkPipeline& outPipeline)
{
	QTDoughApplication* app = QTDoughApplication::instance;
	auto shaderCode = readFile("src/shaders/" + spvName + ".spv");
	VkShaderModule shaderModule = app->CreateShaderModule(shaderCode);

	VkPipelineShaderStageCreateInfo stageInfo{};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shaderModule;
	stageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.stage = stageInfo;

	if (vkCreateComputePipelines(app->_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("MeshGenerator: failed to create sort pipeline: " + spvName);
	}

	vkDestroyShaderModule(app->_logicalDevice, shaderModule, nullptr);
}

