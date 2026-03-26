#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

#define EMITTER_MAX_EVENTS 2048

//Emitter determines the type of emission.
struct Emitter
{
	glm::ivec4 information; // x = owner, y = type of particle... quark, lepton, photon, graphical. z = texture id.
	glm::vec4 position; //Center position of the emitter. w is count.
	glm::vec4 shape; //Radius and shape of the emitter. w is the type.
	glm::vec4 direction; //Direction of emission.
	glm::vec4 velocity; //Speed of emission. w is lifetime.
	glm::vec4 mana; //Also albedo for graphical.
};

class EmitterSystem
{
public:
	EmitterSystem();
	~EmitterSystem();
	static EmitterSystem* instance;

	void InitEmitter(); //Phase 1: create buffers. Call early with other storage buffer creation.
	void InitComputeWorkload(); //Phase 2: create descriptors, pipeline. Call after global descriptors are ready.
	void AddEvent(Emitter event);
	void FlushEvents(); //Upload events to GPU buffer, reset count.
	void Dispatch(VkCommandBuffer commandBuffer);
	void CleanUp();

	Emitter emitterEvents[EMITTER_MAX_EVENTS];
	uint32_t activeEventCount = 0;

private:
	//GPU buffer for emitter events, host-visible.
	VkBuffer emitterEventBuffer = VK_NULL_HANDLE;
	VkDeviceMemory emitterEventBufferMemory = VK_NULL_HANDLE;
	void* mappedEventBuffer = nullptr;

	//Quanta buffer reference (from MaterialSimulation, ping-pong).
	std::vector<VkBuffer>* quantaStorageBuffers = nullptr;
	uint64_t quantaBufferSize = 0;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets; //Per-frame, matches quanta ping-pong.
	VkPipeline emitterPipeline = VK_NULL_HANDLE;
	VkPipelineLayout emitterPipelineLayout = VK_NULL_HANDLE;

	struct EmitterPushConsts {
		uint32_t activeEventCount;
		uint32_t threadsPerEvent;
		float pad0;
		float pad1;
	};
};
