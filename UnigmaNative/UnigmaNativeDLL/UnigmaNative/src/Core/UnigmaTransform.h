#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <../glm/glm.hpp>
#include <iostream>
#include <../glm/gtx/quaternion.hpp>
struct UnigmaTransform
{
	glm::mat4 transformMatrix;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
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

	void UpdateTransform()
	{
		UpdatePosition();
		// Create rotation quaternions
		glm::quat rotX = glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat rotY = glm::angleAxis(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat rotZ = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

		// Combine rotations: Z * Y * X (or change order as needed)
		glm::quat combinedRotation = rotZ * rotY * rotX;

		// Convert quaternion to a 4x4 rotation matrix
		glm::mat4 rotationMatrix = glm::toMat4(combinedRotation);

		// Translation matrix
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

		// Scale matrix
		glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

		// Combine transformations: T * R * S
		transformMatrix = translationMatrix * rotationMatrix * scaleMatrix;
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


	glm::vec3 forward()
	{
		return -glm::vec3(transformMatrix[2][0], transformMatrix[2][1], transformMatrix[2][2]);
	};

	glm::vec3 right()
	{
		return glm::vec3(transformMatrix[0][0], transformMatrix[0][1], transformMatrix[0][2]);
	};

	glm::vec3 up()
	{
		return glm::vec3(transformMatrix[1][0], transformMatrix[1][1], transformMatrix[1][2]);
	};
};