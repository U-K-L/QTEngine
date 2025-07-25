#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "../Core/UnigmaTransform.h"

struct UnigmaCameraStruct
{
	UnigmaTransform _transform;
	glm::vec3 up;
	float aspectRatio;
	glm::vec3 right;
	float fov;

	float nearClip;
	float farClip;
	float orthoWidth;
	float isOrthogonal;


	// Constructor
	UnigmaCameraStruct()
		: _transform(), 
		up(0.0f, 0.0f, 1.0f),
		aspectRatio(16.0f / 9.0f), 
		right(1.0f, 0.0f, 0.0f), 
		fov(45.0f), 
		nearClip(0.1f),
		farClip(1000.0f), 
		orthoWidth(20.0f), 
		isOrthogonal(0) {              
		_transform.transformMatrix[0][0] = 0.685921f; _transform.transformMatrix[0][1] = 0.727676f; _transform.transformMatrix[0][2] = 0.0f; _transform.transformMatrix[0][3] = 0.0f;
		_transform.transformMatrix[1][0] = -0.324013f; _transform.transformMatrix[1][1] = 0.305421f; _transform.transformMatrix[1][2] = 0.895396f; _transform.transformMatrix[1][3] = 0.0f;
		_transform.transformMatrix[2][0] = 0.651558f; _transform.transformMatrix[2][1] = -0.61417f; _transform.transformMatrix[2][2] = 0.445271f; _transform.transformMatrix[2][3] = 0.0f;
		_transform.transformMatrix[3][0] = 7.35889f; _transform.transformMatrix[3][1] = -6.92579f; _transform.transformMatrix[3][2] = 4.95831f; _transform.transformMatrix[3][3] = 1.0f;
	}

	// Overload the assignment operator for UnigmaCameraStruct
	UnigmaCameraStruct& operator=(const UnigmaCameraStruct& other) {
		if (this == &other) {
			return *this; // Handle self-assignment
		}

		this->_transform = other._transform;
		this->up = other.up;
		this->aspectRatio = other.aspectRatio;
		this->right = other.right;
		this->fov = other.fov;
		this->nearClip = other.nearClip;
		this->farClip = other.farClip;
		this->orthoWidth = other.orthoWidth;
		this->isOrthogonal = other.isOrthogonal;

		return *this;
	}

	glm::vec3 position()
	{
		return glm::vec3(_transform.transformMatrix[3][0], _transform.transformMatrix[3][1], _transform.transformMatrix[3][2]);
	};

	glm::vec3 forward()
	{
		return -glm::vec3(_transform.transformMatrix[2][0], _transform.transformMatrix[2][1], _transform.transformMatrix[2][2]);
	};

	void setPosition(const glm::vec3& newPosition)
	{
		_transform.transformMatrix[3][0] = newPosition.x;
		_transform.transformMatrix[3][1] = newPosition.y;
		_transform.transformMatrix[3][2] = newPosition.z;
	}

	void setForward(const glm::vec3& newForward)
	{
		glm::vec3 normalizedForward = glm::normalize(newForward);

		// Update the Z row (negative forward direction)
		_transform.transformMatrix[2][0] = -normalizedForward.x;
		_transform.transformMatrix[2][1] = -normalizedForward.y;
		_transform.transformMatrix[2][2] = -normalizedForward.z;

		// Ensure that the right and up vectors are orthogonal
		glm::vec3 right = glm::normalize(glm::cross(up, normalizedForward));
		glm::vec3 recalculatedUp = glm::cross(normalizedForward, right);

		// Update the matrix's rows for the right and up vectors
		_transform.transformMatrix[0][0] = right.x;
		_transform.transformMatrix[0][1] = right.y;
		_transform.transformMatrix[0][2] = right.z;

		_transform.transformMatrix[1][0] = recalculatedUp.x;
		_transform.transformMatrix[1][1] = recalculatedUp.y;
		_transform.transformMatrix[1][2] = recalculatedUp.z;
	}

	void rotateAroundPoint(const glm::vec3& point, float angle, const glm::vec3& axis)
	{
		// Translate the camera to the origin of rotation
		glm::vec3 currentPosition = position();
		glm::vec3 translatedPosition = currentPosition - point;

		// Apply the rotation
		glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), angle, axis);
		glm::vec3 rotatedPosition = glm::vec3(rotationMatrix * glm::vec4(translatedPosition, 1.0f)) + point;

		// Update the camera's position
		setPosition(rotatedPosition);

		// Update the forward vector
		glm::vec3 newForward = glm::normalize(point - rotatedPosition);
		setForward(newForward);
	}


	glm::mat4 interpolateProjectionMatrix(const glm::mat4& perspective, const glm::mat4& orthographic, float t) {
		// Linearly interpolate between the two matrices
		return (1.0f - t) * perspective + t * orthographic;
	}

	glm::mat4 getVulkanOrthoMatrix(float left, float right, float bottom, float top, float nearZ, float farZ) const {
		glm::mat4 result(1.0f);

		result[0][0] = 2.0f / (right - left);
		result[1][1] = 2.0f / (top - bottom);
		result[2][2] = 1.0f / (farZ - nearZ); // Vulkan-style depth [0, 1]
		result[3][0] = -(right + left) / (right - left);
		result[3][1] = -(top + bottom) / (top - bottom);
		result[3][2] = -nearZ / (farZ - nearZ);
		result[3][3] = 1.0f;

		return result;
	}


	glm::mat4 getProjectionMatrix() {
		isOrthogonal = std::fmin(0.995f, isOrthogonal);
		float orthoHeight = orthoWidth / aspectRatio;
		glm::mat4 ortho = getVulkanOrthoMatrix(
			-orthoWidth / 2.0f, orthoWidth / 2.0f,
			-orthoHeight / 2.0f, orthoHeight / 2.0f,
			nearClip, farClip
		);
		glm::mat4 perspective = glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
		return interpolateProjectionMatrix(perspective, ortho, isOrthogonal);
	}


	//view matrix.
	glm::mat4 getViewMatrix() {
		// Calculate the target point
		glm::vec3 target = position() + forward();

		// Calculate the view matrix
		return glm::lookAt(position(), target, up);
	}

	void Zoom(float delta)
	{
		float zoomFactor = 1.0f;
		if (isOrthogonal > 0) {
			// Adjust orthoWidth
			if (delta > 0) {
				orthoWidth /= (zoomFactor + delta * 0.01);
			}
		}
		else {
			fov = fov + delta;
		}
	}

	void runOrthoDepthDiagnostic(const glm::vec3& worldPosition, const std::string& label = "") {
		glm::mat4 proj;
		if (isOrthogonal >= 0.999f) {
			float orthoHeight = orthoWidth / aspectRatio;
			proj = getVulkanOrthoMatrix(-orthoWidth / 2.0f, orthoWidth / 2.0f,
				-orthoHeight / 2.0f, orthoHeight / 2.0f,
				nearClip, farClip);
			proj[1][1] *= -1.0f; // Flip Y for Vulkan
		}
		else {
			proj = glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
			proj[1][1] *= -1.0f;
		}

		glm::mat4 view = getViewMatrix();

		glm::vec4 worldPos4(worldPosition, 1.0f);
		glm::vec4 viewPos = view * worldPos4;
		glm::vec4 clipPos = proj * viewPos;
		glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

		std::cout << "== Diagnostic for " << label << " ==" << std::endl;
		std::cout << "World Position : (" << worldPosition.x << ", " << worldPosition.y << ", " << worldPosition.z << ")\n";
		std::cout << "View-Space Z   : " << viewPos.z << std::endl;
		std::cout << "Clip-Space Z   : " << clipPos.z << std::endl;
		std::cout << "NDC Z          : " << ndc.z << std::endl;
		std::cout << "Clipped?       : " << ((ndc.z < 0.0f || ndc.z > 1.0f) ? "YES" : "NO") << std::endl;
	}


};