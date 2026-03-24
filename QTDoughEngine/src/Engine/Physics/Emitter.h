#pragma once
#include <glm/glm.hpp>

struct Emitter
{
	glm::vec4 position; //Center position of the emitter. w is count.
	glm::vec4 shape; //Radius and shape of the emitter. w is the type.
	glm::vec4 direction; //Direction of emission.
	glm::vec4 velocity; //Speed of emission. w is lifetime.
};
