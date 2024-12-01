#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <iostream>
#include <glm/gtx/quaternion.hpp>
struct UnigmaTransform
{
	glm::mat4 transformMatrix;
	glm::vec3 position;
	int pad;
	glm::vec3 rotation;
	int pad2;
	glm::vec3 scale;
	int pad3;
	UnigmaTransform() : transformMatrix(1.0f), position(0.0f), rotation(0.0f)
	{
		UpdatePosition();
	}
	// Override the assignment operator to copy from RenderObject's transformMatrix
	UnigmaTransform& operator=(float rObjTransform[16]) {
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				transformMatrix[i][j] = rObjTransform[i * 4 + j];
			}
		}
		return *this;
	}

	// Overload assignment operator for setting position
	UnigmaTransform& operator=(const glm::vec3& newPosition) {
		position = newPosition;
		UpdatePosition(); // Automatically update transformMatrix when position is set
		return *this;
	}

	void UpdatePosition()
	{
		transformMatrix[3][0] = position.x;
		transformMatrix[3][1] = position.y;
		transformMatrix[3][2] = position.z;
	}

	void UpdateRotation()
	{
		// Create quaternion for each axis rotation
		glm::quat rotX = glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // X-axis
		glm::quat rotY = glm::angleAxis(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Y-axis
		glm::quat rotZ = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Z-axis

		// Combine rotations: Z * Y * X (or another order as needed)
		glm::quat combinedRotation = rotZ * rotY * rotX;

		// Convert quaternion to a rotation matrix
		transformMatrix = glm::toMat4(combinedRotation);
	}


	void Print() {
		std::cout << "Transform Matrix:\n";
		for (int i = 0; i < 4; i++) {
			std::cout << "[";
			for (int j = 0; j < 4; j++) {
				std::cout << transformMatrix[i][j] << " ";
			}
			std::cout << "]\n";
		}
	}
};