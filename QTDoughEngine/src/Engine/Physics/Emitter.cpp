#include "Emitter.h"
#include "MaterialSimulationPass.h"

EmitterSystem* EmitterSystem::instance = nullptr;

EmitterSystem::EmitterSystem()
{
}

EmitterSystem::~EmitterSystem()
{
}

void EmitterSystem::InitEmitter()
{
	QTDoughApplication* app = QTDoughApplication::instance;
	VkDevice device = app->_logicalDevice;

	// --- Create host-visible event buffer ---
	uint64_t bufferSize = sizeof(Emitter) * EMITTER_MAX_EVENTS;
	app->CreateBuffer(bufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		emitterEventBuffer, emitterEventBufferMemory);

	vkMapMemory(device, emitterEventBufferMemory, 0, bufferSize, 0, &mappedEventBuffer);

	std::cout << "EmitterSystem buffers created." << std::endl;
}

void EmitterSystem::InitComputeWorkload()
{
	QTDoughApplication* app = QTDoughApplication::instance;
	VkDevice device = app->_logicalDevice;
	MaterialSimulation* matSim = MaterialSimulation::instance;

	quantaStorageBuffers = &matSim->QuantaStorageBuffers;
	quantaBufferSize = sizeof(Quanta) * QUANTA_COUNT;

	if (quantaStorageBuffers->size() < 3)
		throw std::runtime_error("EmitterSystem: QuantaStorageBuffers not fully initialized!");

	std::cout << "emitterEventBuffer: " << emitterEventBuffer << "\n";
	std::cout << "quanta[0]: " << matSim->GetQuantaBuffer(0) << "\n";
	std::cout << "quanta[1]: " << matSim->GetQuantaBuffer(1) << "\n";
	std::cout << "quanta[2]: " << matSim->GetQuantaBuffer(2) << "\n";

	uint64_t eventBufferSize = sizeof(Emitter) * EMITTER_MAX_EVENTS;
	uint32_t frameCount = 3;

	// --- Descriptor set layout (5 bindings: events, quanta read/in, lepton read/in) ---
	std::array<VkDescriptorSetLayoutBinding, 5> bindings{};

	// Binding 0: Emitter events (read).
	bindings[0].binding = 0;
	bindings[0].descriptorCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 1: Quanta Read (read, find free particles).
	bindings[1].binding = 1;
	bindings[1].descriptorCount = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 2: Quanta In (write, claim particles).
	bindings[2].binding = 2;
	bindings[2].descriptorCount = 1;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 3: Lepton Read (read, find free leptons).
	bindings[3].binding = 3;
	bindings[3].descriptorCount = 1;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 4: Lepton In (write, claim leptons).
	bindings[4].binding = 4;
	bindings[4].descriptorCount = 1;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("EmitterSystem: failed to create descriptor set layout!");
	std::cout << "Created Descriptor layout emitter" << std::endl;
	// --- Descriptor pool ---
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 5 * frameCount;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = frameCount;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("EmitterSystem: failed to create descriptor pool!");
	std::cout << "Created Descriptor layout pool" << std::endl;

	// --- Allocate per-frame descriptor sets ---
	std::vector<VkDescriptorSetLayout> layouts(frameCount, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = frameCount;
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(frameCount);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
		throw std::runtime_error("EmitterSystem: failed to allocate descriptor sets!");
	std::cout << "Created Descriptor sets" << std::endl;

	// --- Write descriptors per frame ---
	for (uint32_t i = 0; i < frameCount; i++)
	{

		// In buffer: even frames -> buffer 0, odd frames -> buffer 1.
		uint32_t inIdx = (i % 2 == 0) ? 0 : 1;

		VkDescriptorBufferInfo eventInfo{};
		eventInfo.buffer = emitterEventBuffer;
		eventInfo.offset = 0;
		eventInfo.range = eventBufferSize;

		VkDescriptorBufferInfo quantaReadInfo{};
		quantaReadInfo.buffer = matSim->GetQuantaBuffer(2); // Read buffer is always index 2.
		quantaReadInfo.offset = 0;
		quantaReadInfo.range = quantaBufferSize;

		VkDescriptorBufferInfo quantaInInfo{};
		quantaInInfo.buffer = matSim->GetQuantaBuffer(inIdx);
		quantaInInfo.offset = 0;
		quantaInInfo.range = quantaBufferSize;

		uint64_t leptonBufferSize = sizeof(Lepton) * matSim->leptonMaxSize;

		VkDescriptorBufferInfo leptonReadInfo{};
		leptonReadInfo.buffer = matSim->GetLeptonBuffer(2);
		leptonReadInfo.offset = 0;
		leptonReadInfo.range = leptonBufferSize;

		VkDescriptorBufferInfo leptonInInfo{};
		leptonInInfo.buffer = matSim->GetLeptonBuffer(inIdx);
		leptonInInfo.offset = 0;
		leptonInInfo.range = leptonBufferSize;

		std::array<VkWriteDescriptorSet, 5> writes{};

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].descriptorCount = 1;
		writes[0].pBufferInfo = &eventInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSets[i];
		writes[1].dstBinding = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].descriptorCount = 1;
		writes[1].pBufferInfo = &quantaReadInfo;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = descriptorSets[i];
		writes[2].dstBinding = 2;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].descriptorCount = 1;
		writes[2].pBufferInfo = &quantaInInfo;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = descriptorSets[i];
		writes[3].dstBinding = 3;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[3].descriptorCount = 1;
		writes[3].pBufferInfo = &leptonReadInfo;

		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = descriptorSets[i];
		writes[4].dstBinding = 4;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[4].descriptorCount = 1;
		writes[4].pBufferInfo = &leptonInInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	std::cout << "Created Descriptor writes" << std::endl;

	// --- Pipeline layout: 2 sets (global + emitter) ---
	std::array<VkDescriptorSetLayout, 2> setLayouts = {
		app->globalDescriptorSetLayout,
		descriptorSetLayout
	};

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(EmitterPushConsts);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushRange;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &emitterPipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("EmitterSystem: failed to create pipeline layout!");
	std::cout << "Created pipeline layout" << std::endl;

	// --- Compute pipeline ---
	auto shaderCode = readFile("src/shaders/matsim_emitter.spv");
	VkShaderModule shaderModule = app->CreateShaderModule(shaderCode);

	VkPipelineShaderStageCreateInfo stageInfo{};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shaderModule;
	stageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = emitterPipelineLayout;
	pipelineInfo.stage = stageInfo;

	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &emitterPipeline) != VK_SUCCESS)
		throw std::runtime_error("EmitterSystem: failed to create compute pipeline!");

	vkDestroyShaderModule(device, shaderModule, nullptr);

	std::cout << "EmitterSystem compute workload initialized." << std::endl;
}

void EmitterSystem::AddEvent(Emitter event)
{
	if (activeEventCount >= EMITTER_MAX_EVENTS)
		return;
	emitterEvents[activeEventCount] = event;
	activeEventCount++;
	std::cout << "Added event" << std::endl;
}

void EmitterSystem::FlushEvents()
{
	if (mappedEventBuffer && activeEventCount > 0)
	{
		memcpy(mappedEventBuffer, emitterEvents, sizeof(Emitter) * activeEventCount);
	}
}

void EmitterSystem::Dispatch(VkCommandBuffer commandBuffer)
{
	if (activeEventCount == 0)
		return;

	std::cout << "Emitter dispatch" << std::endl;
	QTDoughApplication* app = QTDoughApplication::instance;
	MaterialSimulation* matSim = MaterialSimulation::instance;


	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, emitterPipeline);

	VkDescriptorSet sets[] = {
		app->globalDescriptorSets[matSim->GetCurrentFrame() % app->globalDescriptorSets.size()],
		descriptorSets[matSim->GetCurrentFrame()]
	};


	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		emitterPipelineLayout, 0, 2, sets, 0, nullptr);
	std::cout << "Emitter descriptor" << std::endl;
	uint32_t threadsPerEvent = 256;
	EmitterPushConsts pc{};
	pc.activeEventCount = activeEventCount;
	pc.threadsPerEvent = threadsPerEvent;

	vkCmdPushConstants(commandBuffer, emitterPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(EmitterPushConsts), &pc);

	uint32_t groupsX = (threadsPerEvent + 63) / 64;
	vkCmdDispatch(commandBuffer, groupsX, activeEventCount, 1);

	VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

	VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dep.memoryBarrierCount = 1;
	dep.pMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &dep);

	activeEventCount = 0;

	std::cout << "Emitter done" << std::endl;
}

void EmitterSystem::CleanUp()
{
	VkDevice device = QTDoughApplication::instance->_logicalDevice;

	if (emitterEventBuffer != VK_NULL_HANDLE)
	{
		vkUnmapMemory(device, emitterEventBufferMemory);
		vkDestroyBuffer(device, emitterEventBuffer, nullptr);
		vkFreeMemory(device, emitterEventBufferMemory, nullptr);
	}

	if (emitterPipeline != VK_NULL_HANDLE)
		vkDestroyPipeline(device, emitterPipeline, nullptr);

	if (emitterPipelineLayout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(device, emitterPipelineLayout, nullptr);

	if (descriptorPool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	if (descriptorSetLayout != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}
