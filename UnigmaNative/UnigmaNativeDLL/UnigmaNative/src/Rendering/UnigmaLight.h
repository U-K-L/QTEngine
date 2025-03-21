#pragma once
#include "..\glm\glm.hpp"

struct UnigmaLight
{
	alignas(16) glm::vec3 position;
	alignas(16) uint32_t GID;
	alignas(16) glm::vec4 emission;
	alignas(16) glm::vec3 direction;
	alignas(16) uint32_t type;
};