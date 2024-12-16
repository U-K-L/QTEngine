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
    VkDeviceSize bufferSize = sizeof(_renderer.vertices[0]) * _renderer.vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(app._logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _renderer.vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(app._logicalDevice, stagingBufferMemory);
    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);

    app.CopyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

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
                _renderer.vertices.push_back(vertex);
            }

            _renderer.indices.push_back(uniqueVertices[vertex]);
        }
    }

}

void UnigmaRenderingObject::CreateIndexBuffer(QTDoughApplication& app)
{
    VkDeviceSize bufferSize = sizeof(_renderer.indices[0]) * _renderer.indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(app._logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _renderer.indices.data(), (size_t)bufferSize);
    vkUnmapMemory(app._logicalDevice, stagingBufferMemory);

    app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);

    app.CopyBuffer(stagingBuffer, _indexBuffer, bufferSize);

    vkDestroyBuffer(app._logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, stagingBufferMemory, nullptr);
}

void UnigmaRenderingObject::Render(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame)
{

    VkBuffer vertexBuffers[] = { _vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.graphicsPipeline);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app._pipelineLayout, 0, 1, &_descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_renderer.indices.size()), 1, 0, 0, 0);

}

void UnigmaRenderingObject::RenderPass(QTDoughApplication& app, VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSet)
{

    VkBuffer vertexBuffers[] = { _vertexBuffer };
    VkDeviceSize offsets[] = { 0 };

    // Combine both global and per-object descriptor sets
    VkDescriptorSet descriptorSetsToBind[] = {
        app.globalDescriptorSet,       // Set 0: Global descriptor set
        _descriptorSets[currentFrame]    // Set 1: Per-object descriptor set
    };


    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind the global descriptor set
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, // First set
        2, // Number of sets
        descriptorSetsToBind,
        0, nullptr // No dynamic offsets
    );

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_renderer.indices.size()), 1, 0, 0, 0);

}

void UnigmaRenderingObject::Cleanup(QTDoughApplication& app)
{
    for (size_t i = 0; i < app.MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(app._logicalDevice, _uniformBuffers[i], nullptr);
        vkFreeMemory(app._logicalDevice, _uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(app._logicalDevice, _descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(app._logicalDevice, _descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(app._logicalDevice, _descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(app._logicalDevice, _descriptorSetLayout, nullptr);

    vkDestroyBuffer(app._logicalDevice, _indexBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, _indexBufferMemory, nullptr);
    vkDestroyBuffer(app._logicalDevice, _vertexBuffer, nullptr);
    vkFreeMemory(app._logicalDevice, _vertexBufferMemory, nullptr);
    vkDestroyBuffer(app._logicalDevice, _vertexBuffer, nullptr);
}

void UnigmaRenderingObject::RenderObjectToUnigma(QTDoughApplication& app, RenderObject& rObj, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& cam)
{

    uRObj._transform = rObj.transformMatrix;
    uRObj.LoadBlenderMeshData(rObj);
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
    //PrintRenderObjectRaw(rObj);

}

void UnigmaRenderingObject::UpdateUniformBuffer(QTDoughApplication& app, uint32_t currentImage, UnigmaRenderingObject& uRObj, UnigmaCameraStruct& camera, void* uniformMem) {

    static auto startTime = std::chrono::high_resolution_clock::now(); //Remove this nasty.
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    //Make transform follow sin and cos.
    ubo.model = uRObj._transform.transformMatrix;
    //rotate camera.
        // Rotate the camera position and forward vector around its up axis
    float rotationSpeed = glm::radians(0.0001f); // Rotate 45 degrees per second
    float angle =  rotationSpeed;

    camera.aspectRatio = app.swapChainExtent.width / (float)app.swapChainExtent.height;

    ubo.view = glm::lookAt(camera.position(), camera.position() + camera.forward(), camera.up);
    ubo.proj = camera.getProjectionMatrix();//glm::ortho(-640.0f/2, 640.0f/2, 520.0f/2, -520.0f/2, 0.01f, 1000.0f);//camera.getProjectionMatrix();
    //ubo.proj = glm::perspective(glm::radians(45.0f), app.swapChainExtent.width / (float)app.swapChainExtent.height, 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1;
    ubo.baseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    if(_material.vectorProperties.count("BaseAlbedo") > 0)
		ubo.baseAlbedo = _material.vectorProperties["BaseAlbedo"];

    ubo.ID = _renderer.GID;

    memcpy(_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));


    //Update int array assignments.

// Determine the size of the array (should be the same as used during buffer creation)
    VkDeviceSize bufferSize = sizeof(uint32_t) * MAX_NUM_TEXTURES;

    for (int i = 0; i < MAX_NUM_TEXTURES; i++)
    {
        if (app.textures.count(_material.textureNames[i]) > 0)
            _material.textureIDs[i] = app.textures[_material.textureNames[i]].ID;
        else
            _material.textureIDs[i] = 0;
    }

    void* data;
    vkMapMemory(app._logicalDevice, intArrayBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _material.textureIDs, bufferSize); // Copy your unsigned int array
    vkUnmapMemory(app._logicalDevice, intArrayBufferMemory);
}

void UnigmaRenderingObject::CreateDescriptorSets(QTDoughApplication& app)
{

    std::vector<VkDescriptorSetLayout> layouts(app.MAX_FRAMES_IN_FLIGHT, _descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    _descriptorSets.resize(app.MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(app._logicalDevice, &allocInfo, _descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < app.MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = _uniformBuffers[i];
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo intArrayBufferInfo{};
        intArrayBufferInfo.buffer = intArrayBuffer;
        intArrayBufferInfo.offset = 0;
        intArrayBufferInfo.range = VK_WHOLE_SIZE; // Use the entire buffer

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = _descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

        // Write for unsigned int array buffer (accessible in all shader stages)
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = _descriptorSets[i];
        descriptorWrites[1].dstBinding = 1; // Match binding in shader
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &intArrayBufferInfo;

        vkUpdateDescriptorSets(app._logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    std::cout << "Created descriptor sets" << std::endl;
}

void UnigmaRenderingObject::CreateDescriptorPool(QTDoughApplication& app)
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(app._logicalDevice, &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::cout << "Created descriptor pools" << std::endl;
}

void UnigmaRenderingObject::CreateUniformBuffers(QTDoughApplication& app)
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    _uniformBuffers.resize(app.MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMemory.resize(app.MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMapped.resize(app.MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < app.MAX_FRAMES_IN_FLIGHT; i++) {
        app.CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);

        vkMapMemory(app._logicalDevice, _uniformBuffersMemory[i], 0, bufferSize, 0, &_uniformBuffersMapped[i]);
    }

}

void UnigmaRenderingObject::CreateDescriptorSetLayout(QTDoughApplication& app)
{
    //Create required buffers.
    //Create Texture IDs
    VkDeviceSize bufferSize = sizeof(uint32_t) * MAX_NUM_TEXTURES;
    app.CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        intArrayBuffer,
        intArrayBufferMemory
    );


    //Bindings for our uniform buffer. Which is things like transform information.
    //Create the actual buffers.
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional


    VkDescriptorSetLayoutBinding intArrayLayoutBinding{};
    intArrayLayoutBinding.binding = 1; // Binding number in the shader
    intArrayLayoutBinding.descriptorCount = 1; // Only one buffer bound here
    intArrayLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // Use storage buffer for an array
    intArrayLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    intArrayLayoutBinding.pImmutableSamplers = nullptr; // Not used for buffer bindings

    //Bind the buffers we specified.
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, intArrayLayoutBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();


    //Create the pools and its information.
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(app.MAX_FRAMES_IN_FLIGHT);


    if (vkCreateDescriptorSetLayout(app._logicalDevice, &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

}

//Graphics pipeline has to be made per shader, and therefore per material using a different shader.
void UnigmaRenderingObject::CreateGraphicsPipeline(QTDoughApplication& app)
{
    printf("Start Creating Graphics Pipeline\n");
    auto vertShaderCode = readFile("src/shaders/vert.spv");
    auto fragShaderCode = readFile("src/shaders/frag.spv");

    std::cout << "Shaders loaded" << std::endl;

    VkShaderModule vertShaderModule = app.CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = app.CreateShaderModule(fragShaderCode);

    std::cout << "Shader modules created and cleaned" << std::endl;

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = app.getBindingDescription();
    auto attributeDescriptions = app.getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(app.swapChainExtent.width);
    viewport.height = static_cast<float>(app.swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = app.swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    std::cout << "Viewport created" << std::endl;

    VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provokingVertexInfo{};
    provokingVertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;
    provokingVertexInfo.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
    rasterizer.pNext = &provokingVertexInfo;

    std::cout << "Rasterizer created" << std::endl;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    std::cout << "Set Dynamic state" << std::endl;

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(app.dynamicStates.size());
    dynamicState.pDynamicStates = app.dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(app._logicalDevice, &pipelineLayoutInfo, nullptr, &app._pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    // Dynamic Rendering Pipeline Setup
    VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &app._swapChainImageFormat;
    pipelineRenderingInfo.depthAttachmentFormat = app.FindDepthFormat();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingInfo; // Point to pipelineRenderingInfo
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = app._pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE; // Not used with dynamic rendering
    pipelineInfo.subpass = 0;

    std::cout << "Dynamic pipeline created" << std::endl;

    if (vkCreateGraphicsPipelines(app._logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &app.graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }


    vkDestroyShaderModule(app._logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(app._logicalDevice, vertShaderModule, nullptr);

    std::cout << "Graphics pipeline created" << std::endl;
}