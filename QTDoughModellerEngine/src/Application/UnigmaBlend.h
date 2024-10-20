#pragma once
// reader.cpp
#include <windows.h>
#include <iostream>
#pragma once
#include <glm/glm.hpp>

#define MAX_VERTICES 34
#define MAX_INDICES 34

struct RenderObject {
	char name[66];
	char _pad[14];
	glm::mat4 transformMatrix;
	int id;
	int vertexCount;
	int indexCount;
	glm::vec3 vertices[MAX_VERTICES];
	uint32_t indices[MAX_INDICES];
	char _pad2[20];

	// Default initialization function
	RenderObject CreateDefaultRenderObject()
	{
		RenderObject obj;

		// Initialize 'name' to an empty string
		obj.name[0] = '\0';

		// Optionally initialize '_pad' to zeros
		memset(obj._pad, 0, sizeof(obj._pad));

		// Initialize 'transformMatrix' to an identity matrix
		obj.transformMatrix = glm::mat4(1.0f);

		// Initialize integer fields to zero
		obj.id = 0;
		obj.vertexCount = 0;
		obj.indexCount = 0;

		// Optionally initialize 'vertices' and 'indices' arrays to zeros
		memset(obj.vertices, 0, sizeof(obj.vertices));
		memset(obj.indices, 0, sizeof(obj.indices));

		return obj;
	}
};

int GatherBlenderInfo();