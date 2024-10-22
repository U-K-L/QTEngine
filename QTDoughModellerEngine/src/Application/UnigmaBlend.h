#pragma once
// reader.cpp
#include <windows.h>
#include <iostream>
#include <iomanip> // For std::setprecision
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> // For glm::value_ptr

#define MAX_VERTICES 272
#define MAX_INDICES 272

struct Vec3 {
	float x;
	float y;
	float z;

	// Default constructor initializes to zero
	Vec3() : x(0.0f), y(0.0f), z(0.0f) {}

	// Constructor to initialize with specific values
	Vec3(float xVal, float yVal, float zVal) : x(xVal), y(yVal), z(zVal) {}

	// Copy constructor
	Vec3(const Vec3& other) : x(other.x), y(other.y), z(other.z) {}

	// Assignment operator
	Vec3& operator=(const Vec3& other) {
		if (this != &other) {
			x = other.x;
			y = other.y;
			z = other.z;
		}
		return *this;
	}
};

#pragma pack(push, 1) // Start struct packing with 1-byte alignment
struct RenderObject {
	char name[66];
	char _pad[14];
	float transformMatrix[16];
	int id;
	int vertexCount;
	int indexCount;
	Vec3 vertices[MAX_VERTICES];
	Vec3 normals[MAX_VERTICES];
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
		for (int i = 0; i < 16; i++)
		{
			obj.transformMatrix[i] = 0;
		}


		// Initialize integer fields to zero
		obj.id = 0;
		obj.vertexCount = 0;
		obj.indexCount = 0;

		return obj;
	}

	// Function to print RenderObject data
	void Print(){
		std::cout << "RenderObject Name: " << name << "\n";
		std::cout << "ID: " << id << "\n";
		std::cout << "Vertex Count: " << vertexCount << "\n";
		std::cout << "Index Count: " << indexCount << "\n";

		std::cout << "Transform Matrix:\n";
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				std::cout << std::setw(10) << transformMatrix[i*4 + j] << " ";
			}
			std::cout << "\n";
		}

		std::cout << "Vertices:\n";
		for (int i = 0; i < vertexCount; i++) {
			std::cout << "  Vertex " << i << ": ("
				<< vertices[i].x << ", "
				<< vertices[i].y << ", "
				<< vertices[i].z << ")\n";
		}

		std::cout << "Indices:\n";
		for (int i = 0; i < indexCount; i++) {
			std::cout << "  Index " << i << ": " << indices[i] << "\n";
		}
	}

	void PrintOffsets()
	{
		std::cout << "Size of RenderObject: " << sizeof(RenderObject) << std::endl;
		std::cout << "Offset of name: " << offsetof(RenderObject, name) << std::endl;
		std::cout << "Offset of _pad: " << offsetof(RenderObject, _pad) << std::endl;
		std::cout << "Offset of transformMatrix: " << offsetof(RenderObject, transformMatrix) << std::endl;
		std::cout << "Offset of id: " << offsetof(RenderObject, id) << std::endl;
		std::cout << "Offset of vertexCount: " << offsetof(RenderObject, vertexCount) << std::endl;
		std::cout << "Offset of indexCount: " << offsetof(RenderObject, indexCount) << std::endl;
		std::cout << "Offset of vertices: " << offsetof(RenderObject, vertices) << std::endl;
		std::cout << "Offset of indices: " << offsetof(RenderObject, indices) << std::endl;
		std::cout << "Offset of _pad2: " << offsetof(RenderObject, _pad2) << std::endl;
	}

};
#pragma pack(pop) // End struct packing

void PrintRenderObjectRaw(const RenderObject& obj);

int GatherBlenderInfo();


extern RenderObject* renderObjects;