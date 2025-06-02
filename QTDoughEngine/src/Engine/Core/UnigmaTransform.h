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

	glm::mat4 GetModelMatrix() const {
		// Recalculate every time for correctness
		glm::quat rotX = glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat rotY = glm::angleAxis(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat rotZ = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
		glm::quat combinedRotation = rotZ * rotY * rotX;

		glm::mat4 rotationMatrix = glm::toMat4(combinedRotation);
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
		glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

		return translationMatrix * rotationMatrix * scaleMatrix;
	}

	glm::mat4 GetModelMatrixBrush() const {
		// Build standard TRS matrix from position, rotation, and scale
		glm::quat rotX = glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat rotY = glm::angleAxis(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat rotZ = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
		glm::quat combinedRotation = rotZ * rotY * rotX;

		glm::mat4 rotationMatrix = glm::toMat4(combinedRotation);
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
		glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

		return translationMatrix * rotationMatrix * scaleMatrix;
	}

	glm::mat4 GetModelMatrixBrush(float desiredWorldSize) const {
		glm::vec3 finalScale = this->scale * (desiredWorldSize * 0.5f);

		glm::quat rotX = glm::angleAxis(rotation.x, glm::vec3(1, 0, 0));
		glm::quat rotY = glm::angleAxis(rotation.y, glm::vec3(0, 1, 0));
		glm::quat rotZ = glm::angleAxis(rotation.z, glm::vec3(0, 0, 1));
		glm::quat rotationQuat = rotZ * rotY * rotX;

		glm::mat4 rotationMatrix = glm::toMat4(rotationQuat);
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
		glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), finalScale);

		return translationMatrix * rotationMatrix * scaleMatrix;
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
};