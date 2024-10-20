#pragma once
#include <string>
#include <cstdint> // For uint32_t
#include "UnigmaMaterial.h"
#include "UnigmaMesh.h"

#include "../../Application/QTDoughApplication.h"

#include "UnigmaRenderingManager.h"

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
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
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	UnigmaRenderingStruct()
		: vertices({ Vertex{ {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} } }),
		indices({ 0 })
	{
	}
};

class UnigmaRenderingObject {
	public:
		UnigmaMesh _mesh;
		UnigmaRenderingStruct _renderer;
		UnigmaMaterial _material;
		VkBuffer _vertexBuffer;
		VkDeviceMemory _vertexBufferMemory;
		VkBuffer _indexBuffer;
		VkDeviceMemory _indexBufferMemory;

		UnigmaRenderingObject()
			: _mesh(), _material(), _renderer()
		{
		}

		UnigmaRenderingObject(UnigmaMesh mesh, UnigmaMaterial mat = UnigmaMaterial())
			: _mesh(mesh), _material(mat), _renderer()
		{
		}
		void Initialization(UnigmaMaterial material);
		void CreateVertexBuffer(QTDoughApplication& app);
		void CreateIndexBuffer(QTDoughApplication& app);
		void Render(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame);
		void LoadModel(UnigmaMesh& mesh);
		void Cleanup(QTDoughApplication& app);
	private:
};