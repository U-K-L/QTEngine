#pragma once
#include "../Core/UnigmaTransform.h"

struct UnigmaCameraStruct
{
	UnigmaTransform _transform;

	UnigmaCameraStruct() : _transform() {}

	glm::vec3 position()
	{
		return glm::vec3(_transform.transformMatrix[3][0], _transform.transformMatrix[3][1], _transform.transformMatrix[3][2]);
	};

	glm::vec3 forward()
	{
		return glm::vec3(_transform.transformMatrix[0][0], _transform.transformMatrix[0][1], _transform.transformMatrix[0][2]);
	};

};