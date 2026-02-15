#include "MaterialSimulationPass.h"

MaterialSimulation* MaterialSimulation::instance = nullptr;

MaterialSimulation::MaterialSimulation()
{

}

MaterialSimulation::~MaterialSimulation()
{

}

void MaterialSimulation::InitMaterialSim()
{
	Field.FieldSize = glm::ivec3(64, 64, 16);
	TileSize = glm::ivec3(8, 8, 8);
	InitQuanta();
	CreateStorageBuffers();
}

void MaterialSimulation::InitComputeWorkload()
{
	CreateComputeDescriptorSetLayout();
	CreateDescriptorPool();
	CreateComputeDescriptorSets();
	CreateComputePipeline();
	std::cout << "MaterialSimulation compute workload initialized." << std::endl;
}

void MaterialSimulation::CreateComputeDescriptorSetLayout()
{
	// Binding 0: Quanta In (read)
	// Binding 1: Quanta Out (write)
	// Binding 2: Quanta Read (read)
	// Binding 3: QuantaIds
	// Binding 4: TileCounts
	// Binding 5: TileOffsets
	// Binding 6: TileCursor

	std::array<VkDescriptorSetLayoutBinding, 7> bindings{};

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
		throw std::runtime_error("MaterialSimulation: failed to create descriptor set layout!");
	}
}

void MaterialSimulation::CreateDescriptorPool()
{
	VkDevice device = QTDoughApplication::instance->_logicalDevice;
	uint32_t frameCount = 3; // Triple buffered.

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 7 * frameCount; // 7 bindings per set.

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = frameCount;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("MaterialSimulation: failed to create descriptor pool!");
	}
}

void MaterialSimulation::CreateComputeDescriptorSets()
{
	VkDevice device = QTDoughApplication::instance->_logicalDevice;
	uint32_t frameCount = 3;

	std::vector<VkDescriptorSetLayout> layouts(frameCount, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = frameCount;
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(frameCount);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("MaterialSimulation: failed to allocate descriptor sets!");
	}

	uint64_t quantaBufferSize = sizeof(Quanta) * QUANTA_COUNT;

	for (uint32_t i = 0; i < frameCount; i++)
	{
		// Ping-pong: even frames 0=In 1=Out, odd frames 1=In 0=Out.
		uint32_t inIdx = (i % 2 == 0) ? 0 : 1;
		uint32_t outIdx = (i % 2 == 0) ? 1 : 0;

		VkDescriptorBufferInfo quantaInInfo{};
		quantaInInfo.buffer = QuantaStorageBuffers[inIdx];
		quantaInInfo.offset = 0;
		quantaInInfo.range = quantaBufferSize;

		VkDescriptorBufferInfo quantaOutInfo{};
		quantaOutInfo.buffer = QuantaStorageBuffers[outIdx];
		quantaOutInfo.offset = 0;
		quantaOutInfo.range = quantaBufferSize;

		VkDescriptorBufferInfo quantaReadInfo{};
		quantaReadInfo.buffer = QuantaStorageBuffers[2];
		quantaReadInfo.offset = 0;
		quantaReadInfo.range = quantaBufferSize;

		VkDescriptorBufferInfo quantaIdsInfo{};
		quantaIdsInfo.buffer = QuantaIdsBuffer[i];
		quantaIdsInfo.offset = 0;
		quantaIdsInfo.range = sizeof(uint32_t) * QUANTA_COUNT;

		VkDescriptorBufferInfo tileCountsInfo{};
		tileCountsInfo.buffer = TileCountsBuffer[i];
		tileCountsInfo.offset = 0;
		tileCountsInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo tileOffsetsInfo{};
		tileOffsetsInfo.buffer = TileOffsetsBuffer[i];
		tileOffsetsInfo.offset = 0;
		tileOffsetsInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo tileCursorInfo{};
		tileCursorInfo.buffer = TileCursorBuffer[i];
		tileCursorInfo.offset = 0;
		tileCursorInfo.range = VK_WHOLE_SIZE;

		std::array<VkWriteDescriptorSet, 7> writes{};
		VkDescriptorBufferInfo* bufferInfos[] = {
			&quantaInInfo, &quantaOutInfo, &quantaReadInfo,
			&quantaIdsInfo, &tileCountsInfo, &tileOffsetsInfo, &tileCursorInfo
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

void MaterialSimulation::CreateComputePipeline()
{
	QTDoughApplication* app = QTDoughApplication::instance;

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
		throw std::runtime_error("MaterialSimulation: failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.stage = stageInfo;

	if (vkCreateComputePipelines(app->_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("MaterialSimulation: failed to create compute pipeline!");
	}

	vkDestroyShaderModule(app->_logicalDevice, shaderModule, nullptr);

	CreateSortPipelines();
}

void MaterialSimulation::CreateComputePipelineFromSPV(const std::string& spvName, VkPipeline& outPipeline)
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
		throw std::runtime_error("MaterialSimulation: failed to create sort pipeline: " + spvName);
	}

	vkDestroyShaderModule(app->_logicalDevice, shaderModule, nullptr);
}

void MaterialSimulation::CreateSortPipelines()
{
	CreateComputePipelineFromSPV("matsim_histogram", histogramPipeline);
	CreateComputePipelineFromSPV("matsim_prefixsum", prefixSumPipeline);
	CreateComputePipelineFromSPV("matsim_scatter", scatterPipeline);
	std::cout << "MaterialSimulation sort pipelines created." << std::endl;
}

void MaterialSimulation::DispatchTileSort(VkCommandBuffer commandBuffer)
{
	QTDoughApplication* app = QTDoughApplication::instance;

	uint32_t tileGridX = Field.FieldSize.x / TileSize.x;
	uint32_t tileGridY = Field.FieldSize.y / TileSize.y;
	uint32_t tileGridZ = Field.FieldSize.z / TileSize.z;
	uint32_t totalTiles = tileGridX * tileGridY * tileGridZ;
	VkDeviceSize tileBufferSize = sizeof(uint32_t) * totalTiles;

	// Clear tileCounts and tileCursor to zero.
	vkCmdFillBuffer(commandBuffer, TileCountsBuffer[currentFrame], 0, tileBufferSize, 0);
	vkCmdFillBuffer(commandBuffer, TileCursorBuffer[currentFrame], 0, tileBufferSize, 0);

	// Barrier: transfer writes -> compute.
	VkMemoryBarrier2 clearBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	clearBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	clearBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	clearBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	clearBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

	VkDependencyInfo clearDep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	clearDep.memoryBarrierCount = 1;
	clearDep.pMemoryBarriers = &clearBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &clearDep);

	// Shared descriptor sets and push constants for all 3 passes.
	VkDescriptorSet sets[] = {
		app->globalDescriptorSets[currentFrame % app->globalDescriptorSets.size()],
		descriptorSets[currentFrame]
	};

	PushConsts pc{};
	pc.particleSize = 1.0f;
	pc.tileGridX = tileGridX;
	pc.tileGridY = tileGridY;
	pc.tileGridZ = tileGridZ;

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts), &pc);

	// Barrier template for between passes.
	VkMemoryBarrier2 passBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	passBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	passBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	passBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	passBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

	VkDependencyInfo passDep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	passDep.memoryBarrierCount = 1;
	passDep.pMemoryBarriers = &passBarrier;

	// Pass 1: Histogram.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, histogramPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);
	vkCmdDispatch(commandBuffer, QUANTA_COUNT / 256, 1, 1);
	vkCmdPipelineBarrier2(commandBuffer, &passDep);

	// Pass 2: Exclusive Prefix Sum.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, prefixSumPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);
	vkCmdDispatch(commandBuffer, 1, 1, 1);
	vkCmdPipelineBarrier2(commandBuffer, &passDep);

	// Pass 3: Scatter.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, scatterPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);
	vkCmdDispatch(commandBuffer, QUANTA_COUNT / 256, 1, 1);
	vkCmdPipelineBarrier2(commandBuffer, &passDep);
}

void MaterialSimulation::Simulate(VkCommandBuffer commandBuffer)
{
	QTDoughApplication* app = QTDoughApplication::instance;

	// Sort quantas into tiles before simulation.
	DispatchTileSort(commandBuffer);

	// Main simulation dispatch.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	PushConsts pc{};
	pc.particleSize = 1.0f;
	pc.tileGridX = Field.FieldSize.x / TileSize.x;
	pc.tileGridY = Field.FieldSize.y / TileSize.y;
	pc.tileGridZ = Field.FieldSize.z / TileSize.z;

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts), &pc);

	VkDescriptorSet sets[] = {
		app->globalDescriptorSets[currentFrame % app->globalDescriptorSets.size()],
		descriptorSets[currentFrame]
	};

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);

	uint32_t groupCount = QUANTA_COUNT / 512; // 8x8x8 = 512 threads per group.
	vkCmdDispatch(commandBuffer, groupCount, 1, 1);

	// Flip ping-pong: 0 -> 1 -> 0 -> 1 ...
	currentFrame = 1 - currentFrame;
}

void MaterialSimulation::CopyOutToRead(VkCommandBuffer commandBuffer)
{
	// After flip, currentFrame == the buffer that was last written to.
	uint32_t outIdx = currentFrame;

	// Copy quanta Out -> READ.
	VkBufferCopy copyRegion{};
	copyRegion.size = quantaMemorySize;
	vkCmdCopyBuffer(commandBuffer, QuantaStorageBuffers[outIdx], QuantaStorageBuffers[2], 1, &copyRegion);

	// Copy tile sort data (written to 1 - currentFrame before flip) -> READ (index 2).
	uint32_t tileIdx = 1 - currentFrame;
	uint32_t totalTiles = (Field.FieldSize.x / TileSize.x) *
	                      (Field.FieldSize.y / TileSize.y) *
	                      (Field.FieldSize.z / TileSize.z);

	VkBufferCopy idsRegion{};
	idsRegion.size = sizeof(uint32_t) * QUANTA_COUNT;
	vkCmdCopyBuffer(commandBuffer, QuantaIdsBuffer[tileIdx], QuantaIdsBuffer[2], 1, &idsRegion);

	VkBufferCopy tileRegion{};
	tileRegion.size = sizeof(uint32_t) * totalTiles;
	vkCmdCopyBuffer(commandBuffer, TileCountsBuffer[tileIdx], TileCountsBuffer[2], 1, &tileRegion);
	vkCmdCopyBuffer(commandBuffer, TileOffsetsBuffer[tileIdx], TileOffsetsBuffer[2], 1, &tileRegion);

	// Barrier: wait for all copies to finish before shaders read READ.
	VkBufferMemoryBarrier barriers[4]{};

	barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].buffer = QuantaStorageBuffers[2];
	barriers[0].offset = 0;
	barriers[0].size = quantaMemorySize;

	barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].buffer = QuantaIdsBuffer[2];
	barriers[1].offset = 0;
	barriers[1].size = idsRegion.size;

	barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[2].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[2].buffer = TileCountsBuffer[2];
	barriers[2].offset = 0;
	barriers[2].size = tileRegion.size;

	barriers[3].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[3].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[3].buffer = TileOffsetsBuffer[2];
	barriers[3].offset = 0;
	barriers[3].size = tileRegion.size;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 4, barriers, 0, nullptr);
}

void MaterialSimulation::InitQuanta()
{
	QTDoughApplication* app = QTDoughApplication::instance;
	quantaMemorySize = sizeof(Quanta) * QUANTA_COUNT;
	std::cout << "Required size for Quanta is: " << quantaMemorySize << std::endl;
	Field.Quantas = (Quanta*)malloc(quantaMemorySize);
	InitQuantaPositions();

	//Allocate on the GPU.
	//Make staging buffers.
	VkBuffer quantaStagingBuffer;
	VkDeviceMemory quantaStagingMemory;
	app->CreateBuffer(quantaMemorySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		quantaStagingBuffer, quantaStagingMemory);

	//Map quanta data to staging buffer.
	void* quantaData;
	vkMapMemory(app->_logicalDevice, quantaStagingMemory, 0, quantaMemorySize, 0, &quantaData);
	memcpy(quantaData, Field.Quantas, quantaMemorySize);
	vkUnmapMemory(app->_logicalDevice, quantaStagingMemory);

	//Create device local buffer for quanta.
	QuantaStorageBuffers.resize(3); //Triple buffering. In, Out, and READ.
	QuantaStorageMemory.resize(3);

	for (int i = 0; i < QuantaStorageBuffers.size(); i++)
	{
		app->CreateBuffer(quantaMemorySize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, QuantaStorageBuffers[i], QuantaStorageMemory[i]);
		app->CopyBuffer(quantaStagingBuffer, QuantaStorageBuffers[i], quantaMemorySize);
	}
}

void MaterialSimulation::InitQuantaPositions()
{
	Quanta* Quantas = Field.Quantas;
	glm::vec3 fs = glm::vec3(Field.FieldSize);
	double volume = (double)fs.x * (double)fs.y * (double)fs.z;
	double step = std::cbrt(volume / (double)QUANTA_COUNT);

	int nx = std::max(1, (int)std::round((double)fs.x / step));
	int ny = std::max(1, (int)std::round((double)fs.y / step));
	int nz = std::max(1, (int)std::round((double)fs.z / step));

	glm::vec3 halfSize = fs * 0.5f;
	float dx = fs.x / (float)nx;
	float dy = fs.y / (float)ny;
	float dz = fs.z / (float)nz;

	uint64_t index = 0;
	for (int z = 0; z < nz && index < QUANTA_COUNT; z++)
	{
		for (int y = 0; y < ny && index < QUANTA_COUNT; y++)
		{
			for (int x = 0; x < nx && index < QUANTA_COUNT; x++)
			{
				float px = (x + 0.5f) * dx - halfSize.x;
				float py = (y + 0.5f) * dy - halfSize.y;
				float pz = (z + 0.5f) * dz - halfSize.z;

				Quantas[index].position = glm::vec4(px, py, pz, 1.0f);
				Quantas[index].resonance = glm::vec4(0.0f);
				Quantas[index].information = glm::ivec4(0);
				Quantas[index].mana = glm::vec4(0.0f);
				index++;
			}
		}
	}

	for (; index < QUANTA_COUNT; index++)
	{
		Quantas[index].position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		Quantas[index].resonance = glm::vec4(0.0f);
		Quantas[index].information = glm::ivec4(0);
		Quantas[index].mana = glm::vec4(0.0f);
	}

	std::cout << "Quanta initialized: " << nx << "x" << ny << "x" << nz
		<< " (" << nx * ny * nz << " grid points, step=" << step << "m)" << std::endl;
}

void MaterialSimulation::CreateStorageBuffers()
{
	QTDoughApplication* app = QTDoughApplication::instance;

	uint32_t tileCountX = Field.FieldSize.x / TileSize.x;
	uint32_t tileCountY = Field.FieldSize.y / TileSize.y;
	uint32_t tileCountZ = Field.FieldSize.z / TileSize.z;
	uint32_t totalTiles = tileCountX * tileCountY * tileCountZ;

	uint32_t frameCount = 3;

	// QuantaIds — one uint per quanta, triple buffered.
	uint64_t quantaIdsSize = sizeof(uint32_t) * QUANTA_COUNT;
	QuantaIds.resize(QUANTA_COUNT, 0);
	QuantaIdsBuffer.resize(frameCount);
	QuantaIdsMemory.resize(frameCount);
	for (uint32_t i = 0; i < frameCount; i++)
	{
		app->CreateBuffer(quantaIdsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, QuantaIdsBuffer[i], QuantaIdsMemory[i]);
	}

	// TileCounts — one uint per tile, triple buffered.
	uint64_t tileCountsSize = sizeof(uint32_t) * totalTiles;
	TileCounts.resize(totalTiles, 0);
	TileCountsBuffer.resize(frameCount);
	TileCountsMemory.resize(frameCount);
	for (uint32_t i = 0; i < frameCount; i++)
	{
		app->CreateBuffer(tileCountsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TileCountsBuffer[i], TileCountsMemory[i]);
	}

	// TileOffsets — one uint per tile, triple buffered.
	TileOffsets.resize(totalTiles, 0);
	TileOffsetsBuffer.resize(frameCount);
	TileOffsetsMemory.resize(frameCount);
	for (uint32_t i = 0; i < frameCount; i++)
	{
		app->CreateBuffer(tileCountsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TileOffsetsBuffer[i], TileOffsetsMemory[i]);
	}

	// TileCursor — one uint per tile, triple buffered.
	TileCursor.resize(totalTiles, 0);
	TileCursorBuffer.resize(frameCount);
	TileCursorMemory.resize(frameCount);
	for (uint32_t i = 0; i < frameCount; i++)
	{
		app->CreateBuffer(tileCountsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TileCursorBuffer[i], TileCursorMemory[i]);
	}

	std::cout << "MaterialSimulation storage buffers created. Tiles: "
		<< tileCountX << "x" << tileCountY << "x" << tileCountZ
		<< " (" << totalTiles << " total)" << std::endl;
}

void MaterialSimulation::SerializeQuantaBlob(const std::string& path)
{
	std::filesystem::path dir = std::filesystem::path(path).parent_path();
	if (!dir.empty())
		std::filesystem::create_directories(dir);
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << "Failed to open blob file for writing: " << path << std::endl;
		return;
	}

	// Header: field size + quanta count.
	uint64_t count = QUANTA_COUNT;
	file.write(reinterpret_cast<const char*>(&Field.FieldSize), sizeof(glm::ivec3));
	file.write(reinterpret_cast<const char*>(&count), sizeof(uint64_t));
	file.write(reinterpret_cast<const char*>(Field.Quantas), sizeof(Quanta) * QUANTA_COUNT);

	file.close();
	std::cout << "Quanta blob serialized to: " << path << " (" << sizeof(Quanta) * QUANTA_COUNT << " bytes)" << std::endl;
}

void MaterialSimulation::SerializeQuantaText(const std::string& path)
{
	std::filesystem::path dir = std::filesystem::path(path).parent_path();
	if (!dir.empty())
		std::filesystem::create_directories(dir);
	std::ofstream file(path);
	if (!file.is_open())
	{
		std::cerr << "Failed to open text file for writing: " << path << std::endl;
		return;
	}

	file << "FieldSize: " << Field.FieldSize.x << " " << Field.FieldSize.y << " " << Field.FieldSize.z << "\n";
	file << "QuantaCount: " << QUANTA_COUNT << "\n";

	Quanta* Quantas = Field.Quantas;
	for (uint64_t i = 0; i < QUANTA_COUNT; i++)
	{
		const Quanta& q = Quantas[i];
		file << "[" << i << "] "
			<< "pos(" << q.position.x << " " << q.position.y << " " << q.position.z << " " << q.position.w << ") "
			<< "res(" << q.resonance.x << " " << q.resonance.y << " " << q.resonance.z << " " << q.resonance.w << ") "
			<< "inf(" << q.information.x << " " << q.information.y << " " << q.information.z << " " << q.information.w << ") "
			<< "mana(" << q.mana.x << " " << q.mana.y << " " << q.mana.z << " " << q.mana.w << ")\n";
	}

	file.close();
	std::cout << "Quanta text serialized to: " << path << std::endl;
}

void MaterialSimulation::DeserializeQuantaBlob(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << "Failed to open blob file for reading: " << path << std::endl;
		return;
	}

	glm::ivec3 loadedFieldSize;
	uint64_t loadedCount;
	file.read(reinterpret_cast<char*>(&loadedFieldSize), sizeof(glm::ivec3));
	file.read(reinterpret_cast<char*>(&loadedCount), sizeof(uint64_t));

	if (loadedCount != QUANTA_COUNT)
	{
		std::cerr << "Blob quanta count mismatch: file has " << loadedCount
			<< " but expected " << QUANTA_COUNT << std::endl;
		file.close();
		return;
	}

	Field.FieldSize = loadedFieldSize;
	file.read(reinterpret_cast<char*>(Field.Quantas), sizeof(Quanta) * QUANTA_COUNT);

	file.close();
	std::cout << "Quanta blob deserialized from: " << path
		<< " (field " << Field.FieldSize.x << "x" << Field.FieldSize.y << "x" << Field.FieldSize.z << ")" << std::endl;
}