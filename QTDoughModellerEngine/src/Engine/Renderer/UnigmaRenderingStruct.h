#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <limits> 
#include <iostream>

#include "UnigmaMaterial.h"
#include "../Core/UnigmaTransform.h"

struct Vertex {
	glm::vec3 pos = glm::vec3(0.0f);
	glm::vec3 color = glm::vec3(1.0f);              // Default color (white)
	glm::vec2 texCoord = glm::vec2(0.0f);
	glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default normal (pointing along +Z axis)

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}


struct UnigmaRenderingStruct
{
    uint32_t GID;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	UnigmaRenderingStruct()
		: vertices({ Vertex
            // Front face
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

            // Back face
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

            // Left face
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

            // Right face
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

            // Top face
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

            // Bottom face
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
            }),
        indices({
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        8, 9, 10, 10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Top face
        16, 17, 18, 18, 19, 16,
        // Bottom face
        20, 21, 22, 22, 23, 20
            })
    {
    }

	// Copy assignment operator
	UnigmaRenderingStruct& operator=(const UnigmaRenderingStruct& other) {
		if (this == &other) {
			return *this; // Handle self-assignment
		}

		// Copy data
		this->vertices = other.vertices;
		this->indices = other.indices;

		return *this;
	}

	//Print function. For debugging purposes
	void Print()
	{
		for (int i = 0; i < vertices.size(); i++)
		{
			std::cout << "Vertex " << i << ":\n";
			std::cout << "Position: " << vertices[i].pos.x << " " << vertices[i].pos.y << " " << vertices[i].pos.z << "\n";
			std::cout << "Color: " << vertices[i].color.x << " " << vertices[i].color.y << " " << vertices[i].color.z << "\n";
			std::cout << "Tex Coords: " << vertices[i].texCoord.x << " " << vertices[i].texCoord.y << "\n";
			std::cout << "Normal: " << vertices[i].normal.x << " " << vertices[i].normal.y << " " << vertices[i].normal.z << "\n";
		}
	}
};

struct UnigmaRenderingStructCopyableAttributes
{
    bool isRendering = false;
    UnigmaMaterial _material;
    UnigmaRenderingStruct _renderer;
    UnigmaTransform _transform;

    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VkDeviceMemory> _uniformBuffersMemory;
    std::vector<void*> _uniformBuffersMapped;
    VkDescriptorSetLayout _descriptorSetLayout;
    VkDescriptorPool _descriptorPool;
    std::vector<VkDescriptorSet> _descriptorSets;

    UnigmaRenderingStructCopyableAttributes()
        : _material(), _renderer(), _transform()
    {
    }
};