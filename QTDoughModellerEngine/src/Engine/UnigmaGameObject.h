#pragma once
#include <glm/glm.hpp>

struct UnigmaGameObject
{
	char name[66];
	char _pad[14];
	glm::mat4 transformMatrix;
	int id;
	char _pad2[12];
};