#include "UnigmaRenderingObject.h"
#include "../../Application/QTDoughApplication.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

bool ContainsSubstring(const char* charArray, const char* substring) {
    // Use strstr to check if the substring is present in charArray
    return strstr(charArray, substring) != nullptr;
}

void UnigmaRenderingObject::Initialization(UnigmaMaterial material)
{
    _material = material;
}

void UnigmaRenderingObject::CreateVertexBuffer(QTDoughApplication& app)
{
    //_renderer.PrintAllInfo();

    std::vector<Vertex> vertices2 = { Vertex

                {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    };

    VkDeviceSize bufferSize = sizeof(vertices2[0]) * vertices2.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // Log staging buffer info
    std::cout << "Vertex Staging Buffer Created:\n";
    std::cout << "  Buffer Handle: " << stagingBuffer << "\n";
    std::cout << "  Buffer Size: " << bufferSize << "\n";
    std::cout << "  Memory Handle: " << stagingBufferMemory << "\n";

    void* data;
    vkMapMemory(app._logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _renderer.vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(app._logicalDevice, stagingBufferMemory);
    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);

    app.CopyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

    // Log vertex buffer info
    std::cout << "Vertex Buffer Created:\n";
    std::cout << "  Buffer Handle: " << _vertexBuffer << "\n";
    std::cout << "  Buffer Size: " << bufferSize << "\n";
    std::cout << "  Memory Handle: " << _vertexBufferMemory << "\n";

    vkDestroyBuffer(app._logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, stagingBufferMemory, nullptr);
}

void UnigmaRenderingObject::LoadModel(UnigmaMesh& mesh)
{
    _renderer.indices.clear();
    _renderer.vertices.clear();
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::string path = AssetsPath + mesh.MODEL_PATH.c_str();

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(_renderer.vertices.size());
               // _renderer.vertices.push_back(vertex);
            }

            _renderer.indices.push_back(uniqueVertices[vertex]);
            std::cout << _renderer.vertices.size() << std::endl;
        }
        

    }

}

void UnigmaRenderingObject::CreateIndexBuffer(QTDoughApplication& app)
{
    //_renderer.PrintAllInfo();

    std::vector<uint32_t> indices2 = { 0, 1, 2, 2, 3, 0};

    VkDeviceSize bufferSize = sizeof(indices2[0]) * indices2.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    // Log staging buffer info
    std::cout << "Index Staging Buffer Created:\n";
    std::cout << "  Buffer Handle: " << stagingBuffer << "\n";
    std::cout << "  Buffer Size: " << bufferSize << "\n";
    std::cout << "  Memory Handle: " << stagingBufferMemory << "\n";

    void* data;
    vkMapMemory(app._logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _renderer.indices.data(), (size_t)bufferSize);
    vkUnmapMemory(app._logicalDevice, stagingBufferMemory);

    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);

    app.CopyBuffer(stagingBuffer, _indexBuffer, bufferSize);

    // Log index buffer info
    std::cout << "Index Buffer Created:\n";
    std::cout << "  Buffer Handle: " << _indexBuffer << "\n";
    std::cout << "  Buffer Size: " << bufferSize << "\n";
    std::cout << "  Memory Handle: " << _indexBufferMemory << "\n";

    vkDestroyBuffer(app._logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, stagingBufferMemory, nullptr);
}

void UnigmaRenderingObject::Render(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame)
{
    //_renderer.PrintAllInfo();
    VkBuffer vertexBuffers[] = { _vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app._pipelineLayout, 0, 1, &app._descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_renderer.indices.size()), 1, 0, 0, 0);
}

void UnigmaRenderingObject::Cleanup(QTDoughApplication& app)
{
    vkDestroyBuffer(app._logicalDevice, _indexBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, _indexBufferMemory, nullptr);
    vkDestroyBuffer(app._logicalDevice, _vertexBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, _vertexBufferMemory, nullptr);
    vkDestroyBuffer(app._logicalDevice, _vertexBuffer, nullptr);
}

void UnigmaRenderingObject::RenderObjectToUnigma(QTDoughApplication& app, RenderObject& rObj, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& cam)
{
    //Get transform matrix.
    //uRObj._transform = rObj.transformMatrix;
    //uRObj.LoadBlenderMeshData(rObj);
    uRObj.CreateVertexBuffer(app);
    uRObj.CreateIndexBuffer(app);
}

void UnigmaRenderingObject::LoadBlenderMeshData(RenderObject& rObj)
{
    _renderer.indices.clear();
    _renderer.vertices.clear();

    uint32_t* indices = rObj.indices;
    Vec3* vertices = rObj.vertices;
    Vec3* normals = rObj.normals;
    int indexCount = rObj.indexCount;
    for (int i = 0; i < indexCount; i++)
    {
        Vertex vertex{};

        vertex.pos.x = vertices[i].x;
        vertex.pos.y = vertices[i].y;
        vertex.pos.z = vertices[i].z;

        vertex.normal.x = normals[i].x;
        vertex.normal.y = normals[i].y;
        vertex.normal.z = normals[i].z;

        vertex.texCoord = {
            0.0f,0.0f
            //attrib.texcoords[2 * index.texcoord_index + 0],
            //1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
        };

        vertex.color = { 1.0f, 1.0f, 1.0f };

        _renderer.vertices.push_back(vertex);

        _renderer.indices.push_back(indices[i]);
    }

}

void UnigmaRenderingObject::UpdateUniformBuffer(QTDoughApplication& app, uint32_t currentImage, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& camera) {

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    //ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.model = uRObj._transform.transformMatrix;
            ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    //ubo.view = glm::lookAt(camera.position(), camera.position() + camera.forward(), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(145.0f), app.swapChainExtent.width / (float)app.swapChainExtent.height, 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1;

    memcpy(app._uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void UnigmaRenderingObject::Print()
{
    _renderer.PrintAllInfo();
}