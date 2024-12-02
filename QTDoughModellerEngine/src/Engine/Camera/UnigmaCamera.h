#pragma once
#include "../Core/UnigmaTransform.h"

struct UnigmaCameraStruct
{
	UnigmaTransform _transform;
	glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
	/*
	* Default
	* 
	* [0.685921 0.727676 0 0 ]
	* [-0.324013 0.305421 0.895396 0 ]
	* [0.651558 -0.61417 0.445271 0 ]
	* [7.35889 -6.92579 4.95831 1 ]
	* 
	*/

	UnigmaCameraStruct() : _transform() {
		// Set matrix to match the specified default values
		_transform.transformMatrix[0][0] = 0.685921f; _transform.transformMatrix[0][1] = 0.727676f; _transform.transformMatrix[0][2] = 0.0f; _transform.transformMatrix[0][3] = 0.0f;
		_transform.transformMatrix[1][0] = -0.324013f; _transform.transformMatrix[1][1] = 0.305421f; _transform.transformMatrix[1][2] = 0.895396f; _transform.transformMatrix[1][3] = 0.0f;
		_transform.transformMatrix[2][0] = 0.651558f; _transform.transformMatrix[2][1] = -0.61417f; _transform.transformMatrix[2][2] = 0.445271f; _transform.transformMatrix[2][3] = 0.0f;
		_transform.transformMatrix[3][0] = 7.35889f; _transform.transformMatrix[3][1] = -6.92579f; _transform.transformMatrix[3][2] = 4.95831f; _transform.transformMatrix[3][3] = 1.0f;
	}

	glm::vec3 position()
	{
		return glm::vec3(_transform.transformMatrix[3][0], _transform.transformMatrix[3][1], _transform.transformMatrix[3][2]);
	};

	glm::vec3 forward()
	{
		return -glm::vec3(_transform.transformMatrix[2][0], _transform.transformMatrix[2][1], _transform.transformMatrix[2][2]);
	};

};