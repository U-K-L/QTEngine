#include "VoxelizerPass.h"
#include <random>

VoxelizerPass* VoxelizerPass::instance = nullptr;

VoxelizerPass::~VoxelizerPass() {
    PassName = "VoxelizerPass";
}

VoxelizerPass::VoxelizerPass() {
    VOXEL_COUNT = VOXEL_RESOLUTION * VOXEL_RESOLUTION * VOXEL_RESOLUTION;
    PassName = "VoxelizerPass";
    PassNames.push_back("VoxelizerPass");
}

void VoxelizerPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("voxelizer");
}

std::vector<VoxelizerPass::Triangle> VoxelizerPass::ExtractTrianglesFromMeshFromTriplets(const std::vector<ComputeVertex>& vertices, const std::vector<glm::uvec3>& triangleIndices)
{
    std::vector<Triangle> triangles;

    for (const auto& tri : triangleIndices)
    {
        if (tri.x >= vertices.size() || tri.y >= vertices.size() || tri.z >= vertices.size()) {
            std::cerr << "[Warning] Skipping invalid triangle: " << tri.x << ", " << tri.y << ", " << tri.z << std::endl;
            continue;
        }

        glm::vec3 a = glm::vec3(vertices[tri.x].position);
        glm::vec3 b = glm::vec3(vertices[tri.y].position);
        glm::vec3 c = glm::vec3(vertices[tri.z].position);
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));

        triangles.push_back({ a, b, c, normal });
    }

    return triangles;
}


float VoxelizerPass::DistanceToTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    // Algorithm based on Inigo Quilez's "Signed distance functions"
    glm::vec3 ba = b - a, pa = p - a;
    glm::vec3 cb = c - b, pb = p - b;
    glm::vec3 ac = a - c, pc = p - c;
    glm::vec3 nor = glm::cross(ba, ac);

    float sign = glm::sign(glm::dot(glm::cross(ba, nor), pa) +
        glm::dot(glm::cross(cb, nor), pb) +
        glm::dot(glm::cross(ac, nor), pc));

    float d = glm::min(glm::min(
        glm::length(ba * glm::clamp(glm::dot(ba, pa) / glm::dot(ba, ba), 0.0f, 1.0f) - pa),
        glm::length(cb * glm::clamp(glm::dot(cb, pb) / glm::dot(cb, cb), 0.0f, 1.0f) - pb)),
        glm::length(ac * glm::clamp(glm::dot(ac, pc) / glm::dot(ac, ac), 0.0f, 1.0f) - pc));

    float distToPlane = glm::abs(glm::dot(nor, pa)) / glm::length(nor);
    bool inside = sign >= 2.0f;

    return inside ? distToPlane : d;
}

void VoxelizerPass::BakeSDFFromTriangles()
{
    //Get total number of voxels.
    auto voxelCount = voxels.size();
    std::cout << "Voxel Count: " << voxelCount << std::endl;

    float currentPercentage = 0.0f;

    for (auto& voxel : voxels)
    {
        float minDistance = 100.0f;
        glm::vec3 voxelCenter = glm::vec3(voxel.positionDistance);

        for (const auto& tri : triangleSoup)
        {
            float d = DistanceToTriangle(voxelCenter, tri.a, tri.b, tri.c);
            minDistance = std::min(minDistance, d);
        }

        voxel.positionDistance.w = minDistance;

        //If done around 1% of the voxels, print out the progress.
        float percentage = (float)(&voxel - &voxels[0]) / (float)voxels.size();
        if (percentage >= currentPercentage)
        {
            std::cout << "Voxelization progress: " << percentage * 100.0f << "%" << std::endl;
            currentPercentage += 0.01f;
        }
    }
}

void VoxelizerPass::CreateComputePipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    auto computeShaderCode = readFile("src/shaders/voxelizer.spv");
    std::cout << "Creating compute pipeline" << std::endl;
    VkShaderModule computeShaderModule = app->CreateShaderModule(computeShaderCode);

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;

    std::array<VkDescriptorSetLayout, 2> layouts = {
        app->globalDescriptorSetLayout,     // global information (camera, images, etc.)
        computeDescriptorSetLayout          // set 1 (particles, etc.)
    };

    VkPipelineLayoutCreateInfo pli{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pli.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pli.pSetLayouts = layouts.data();


    if (vkCreatePipelineLayout(app->_logicalDevice, &pli, nullptr, &computePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(app->_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(app->_logicalDevice, computeShaderModule, nullptr);
    std::cout << "Compute pipeline created" << std::endl;

    std::cout << "Memory of voxels: " << sizeof(Voxel) * VOXEL_COUNT / 1024.0f / 1024.0f << " MB" << std::endl;
    std::cout << "Size of voxel: " << sizeof(Voxel) << " bytes" << std::endl;

    readbackBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    readbackBufferMemories.resize(app->MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; ++i) {
        app->CreateBuffer(
            sizeof(Voxel) * VOXEL_COUNT,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            readbackBuffers[i],
            readbackBufferMemories[i]
        );
    }
    frameReadbackData.resize(app->MAX_FRAMES_IN_FLIGHT);
    for (auto& frame : frameReadbackData)
        frame.resize(VOXEL_COUNT);

    std::cout << "Readback created: " << PassName << std::endl;
}

void VoxelizerPass::CreateComputeDescriptorSets()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    std::cout << "Creating Compute Descriptor Sets" << std::endl;
    std::vector<VkDescriptorSetLayout> layouts(app->MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(app->MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    computeDescriptorSets.resize(app->MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(app->_logicalDevice, &allocInfo, computeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = _uniformBuffers[i];
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo intArrayBufferInfo{};
        intArrayBufferInfo.buffer = intArrayBuffer;
        intArrayBufferInfo.offset = 0;
        intArrayBufferInfo.range = VK_WHOLE_SIZE;


        std::array<VkWriteDescriptorSet, 6> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = computeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = computeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &intArrayBufferInfo;


        VkDescriptorBufferInfo storageBufferInfoLastFrame{};
        storageBufferInfoLastFrame.buffer = shaderStorageBuffers[(i - 1) % app->MAX_FRAMES_IN_FLIGHT];
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = sizeof(Voxel) * VOXEL_COUNT;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = computeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoLastFrame;

        VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
        storageBufferInfoCurrentFrame.buffer = shaderStorageBuffers[i];
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = sizeof(Voxel) * VOXEL_COUNT;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = computeDescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &storageBufferInfoCurrentFrame;

        VkDescriptorBufferInfo iInfo{};
        VkDescriptorBufferInfo vInfo{};
        vInfo.buffer = vertexBuffer;
        vInfo.offset = 0;
        vInfo.range = VK_WHOLE_SIZE;

        iInfo.buffer = indexBuffer;
        iInfo.offset = 0;
        iInfo.range = VK_WHOLE_SIZE;

        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = computeDescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[4].pBufferInfo = &vInfo;

        descriptorWrites[5] = descriptorWrites[4];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].pBufferInfo = &iInfo;

        vkUpdateDescriptorSets(app->_logicalDevice, 6, descriptorWrites.data(), 0, nullptr);
    }

    std::cout << "Created descriptor sets" << std::endl;
}

void VoxelizerPass::IsOccupiedByVoxel()
{
    float voxelSize = SCENE_BOUNDS / (float)VOXEL_RESOLUTION;
    for (auto& voxel : voxels)
    {
        //voxel.occuipiedInfo = glm::vec4(0.0f);
        for (auto& vertex : vertices)
        {

            glm::vec3 pos = voxel.positionDistance;
            glm::vec3 vPos = vertex.position;

            glm::vec3 halfSize = glm::vec3(voxel.normalDensity.w * 0.5f);
            glm::vec3 min = pos - halfSize;
            glm::vec3 max = pos + halfSize;

            if (vPos.x >= min.x && vPos.x <= max.x &&
                vPos.y >= min.y && vPos.y <= max.y &&
                vPos.z >= min.z && vPos.z <= max.z)
            {
                //voxel.occuipiedInfo = glm::vec4(1.0f);
            }

        }
    }
}

void VoxelizerPass::UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) {

    QTDoughApplication* app = QTDoughApplication::instance;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.proj = glm::mat4(1.0f);
    ubo.texelSize = glm::vec2(1.0f / app->swapChainExtent.width, 1.0f / app->swapChainExtent.height);

    ubo.view = glm::lookAt(CameraMain.position(), CameraMain.position() + CameraMain.forward(), CameraMain.up);
    ubo.proj = CameraMain.getProjectionMatrix();

    //print view matrix.
    //Update int array assignments.

    // Determine the size of the array (should be the same as used during buffer creation)
    VkDeviceSize bufferSize = sizeof(uint32_t) * MAX_NUM_TEXTURES;

    for (int i = 0; i < MAX_NUM_TEXTURES; i++)
    {
        if (app->textures.count(material.textureNames[i]) > 0)
        {
            material.textureIDs[i] = app->textures[material.textureNames[i]].ID;
        }
        else
            material.textureIDs[i] = 0;
    }

    memcpy(_uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

    void* data;
    vkMapMemory(app->_logicalDevice, intArrayBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, material.textureIDs, bufferSize); // Copy your unsigned int array
    vkUnmapMemory(app->_logicalDevice, intArrayBufferMemory);


    vertices.clear();
    //triangleSoup.clear();
    //indices.clear();
    //Update all vertices world positions.
    for (int i = 0; i < renderingObjects.size(); i++)
    {
        //std::cout << "Index: " << i << "count: " << renderingObjects[i]->_renderer.vertices.size() << std::endl;

        float density = SCENE_BOUNDS / (float)VOXEL_RESOLUTION;
        density *= 0.5f; // half the size of the voxel.

        if(i == 4)
			density *= 4.5f;
        if (i == 3)
            density *= 4.5f;

        glm::mat4 model = renderingObjects[i]->_transform.GetModelMatrix();
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model))); // for correct normal transform

        for (auto& vertex : renderingObjects[i]->_renderer.vertices)
        {
            ComputeVertex computeVertex;
            computeVertex.position = model * glm::vec4(vertex.pos, 1.0f);
            computeVertex.texCoord = glm::vec4(vertex.texCoord, 0.0f, density);
            computeVertex.normal = glm::vec4(normalMatrix * vertex.normal, 0.0f); // use normal matrix

            vertices.push_back(computeVertex);
        }
    }

    //std::cout << "Vertices count " << vertices.size() << std::endl;


    /*
    //All of BELOW will move to GPU side ------------------------------------
    // MOVE TO GPU ----------------------------------------------

    triangleIndices.resize(indices.size() / 3);
    for (size_t i = 0; i < triangleIndices.size(); ++i)
    {
        triangleIndices[i] = glm::uvec3(indices[i * 3], indices[i * 3 + 1], indices[i * 3 + 2]);
    }
    auto meshTriangles = ExtractTrianglesFromMeshFromTriplets(vertices, triangleIndices);
    triangleSoup.insert(triangleSoup.end(), meshTriangles.begin(), meshTriangles.end());
    */
    //Bake SDF from triangles.
    //BakeSDFFromTriangles();

    //std::cout << "Indices count " << indices.size() << std::endl;

    VkDeviceSize vertexBufferSize = sizeof(ComputeVertex) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    app->CreateBuffer(vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(app->_logicalDevice, stagingMemory, 0, vertexBufferSize, 0, &mapped);
    memcpy(mapped, vertices.data(), vertexBufferSize);
    vkUnmapMemory(app->_logicalDevice, stagingMemory);

    app->CopyBuffer(stagingBuffer, vertexBuffer, vertexBufferSize);

    vkDestroyBuffer(app->_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingMemory, nullptr);


    //Update Voxel Information
    //IsOccupiedByVoxel();

    /*
    //Update voxels.
    VkDeviceSize voxelBufferSize = sizeof(Voxel) * VOXEL_COUNT;
    VkBuffer stagingBuffer2;
    VkDeviceMemory stagingMemory2;
    app->CreateBuffer(voxelBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer2, stagingMemory2);

    void* mapped2;
    vkMapMemory(app->_logicalDevice, stagingMemory2, 0, voxelBufferSize, 0, &mapped2);
    memcpy(mapped2, voxels.data(), voxelBufferSize);
    vkUnmapMemory(app->_logicalDevice, stagingMemory2);

    app->CopyBuffer(stagingBuffer2, shaderStorageBuffers[currentFrame], voxelBufferSize);

    vkDestroyBuffer(app->_logicalDevice, stagingBuffer2, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingMemory2, nullptr);
    */
    //All of ABOVE will move to GPU side ------------------------------------
    // MOVE TO GPU ----------------------------------------------

}


void VoxelizerPass::CreateComputeDescriptorSetLayout()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    //Create required buffers.
    //Create Texture IDs
    VkDeviceSize bufferSize = sizeof(uint32_t) * MAX_NUM_TEXTURES;
    app->CreateBuffer(
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
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional


    //For texture IDs the second binding.
    VkDescriptorSetLayoutBinding intArrayLayoutBinding{};
    intArrayLayoutBinding.binding = 1;
    intArrayLayoutBinding.descriptorCount = 1;
    intArrayLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    intArrayLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    intArrayLayoutBinding.pImmutableSamplers = nullptr;

    //Third binding is for storage buffer. General purpose.
    VkDescriptorSetLayoutBinding generalBinding{};
    generalBinding.binding = 2;
    generalBinding.descriptorCount = 1;
    generalBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    generalBinding.pImmutableSamplers = nullptr;
    generalBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Fourth binding is for storage buffer. General purpose.
    VkDescriptorSetLayoutBinding generalBinding2{};
    generalBinding2.binding = 3;
    generalBinding2.descriptorCount = 1;
    generalBinding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    generalBinding2.pImmutableSamplers = nullptr;
    generalBinding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Fith binding for vertex buffer.
    VkDescriptorSetLayoutBinding vertexBinding{};
    vertexBinding.binding = 4;
    vertexBinding.descriptorCount = 1;
    vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBinding.pImmutableSamplers = nullptr;
    vertexBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //sixth binding for index buffer.
    VkDescriptorSetLayoutBinding indexBinding{};
    indexBinding.binding = 5;
    indexBinding.descriptorCount = 1;
    indexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexBinding.pImmutableSamplers = nullptr;
    indexBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Bind the buffers we specified.
    std::array<VkDescriptorSetLayoutBinding, 6> bindings = { uboLayoutBinding, intArrayLayoutBinding, generalBinding, generalBinding2, vertexBinding, indexBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(app->_logicalDevice, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }

}

void VoxelizerPass::CreateShaderStorageBuffers()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    //Create the triangle soup first.
    CreateTriangleSoup();
    // Initial data. This should create the voxel scene grid.
    voxels.resize(VOXEL_COUNT);

    for (int i = 0; i < VOXEL_RESOLUTION; i++)
    {
        for (int j = 0; j < VOXEL_RESOLUTION; j++)
        {
            for (int k = 0; k < VOXEL_RESOLUTION; k++)
            {
                auto& voxel = voxels[i * VOXEL_RESOLUTION * VOXEL_RESOLUTION + j * VOXEL_RESOLUTION + k];

                float voxelSize = SCENE_BOUNDS / (float)VOXEL_RESOLUTION;

                voxel.positionDistance = glm::vec4(
                    (i + 0.5f) * voxelSize - SCENE_BOUNDS * 0.5f,
                    (j + 0.5f) * voxelSize - SCENE_BOUNDS * 0.5f,
                    (k + 0.5f) * voxelSize - SCENE_BOUNDS * 0.5f,
                    defaultDistanceMax
                );

                voxel.normalDensity = glm::vec4(0.0f, 0.0f, 0.0f, voxelSize);

            }
        }
    }


    for (auto& voxel : voxels) {
        /*
        float x = (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float y = (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float z = (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        glm::vec3 dir = glm::normalize(glm::vec3(x, y, z));
        float r = std::cbrt(std::rand() / (float)RAND_MAX) * 10.0f;

        glm::vec3 pos = abs(dir * r);
        voxel.positionDistance = glm::vec4(pos, 0.001f);
        voxel.normalDensity = glm::vec4(0.5f, 1.0f, 0.0f, 2.0f);
        */

    }

    VkDeviceSize bufferSize = sizeof(Voxel) * VOXEL_COUNT;

    // Create a staging buffer used to upload data to the gpu
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(app->_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, voxels.data(), voxels.size() * sizeof(Voxel));
    vkUnmapMemory(app->_logicalDevice, stagingBufferMemory);

    shaderStorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    shaderStorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);

    // Copy initial data to all storage buffers
    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        app->CreateBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shaderStorageBuffers[i], shaderStorageBuffersMemory[i]);
        app->CopyBuffer(stagingBuffer, shaderStorageBuffers[i], bufferSize);
    }

    vkDestroyBuffer(app->_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBufferMemory, nullptr);

}


void VoxelizerPass::Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
    QTDoughApplication* app = QTDoughApplication::instance;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

    VkDescriptorSet sets[] = {
        app->globalDescriptorSets[currentFrame],
        computeDescriptorSets[currentFrame]
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        computePipelineLayout,
        0, 2, sets,
        0, nullptr);

    uint32_t groupCountX = (VOXEL_RESOLUTION + 7) / 8;
    uint32_t groupCountY = (VOXEL_RESOLUTION + 7) / 8;
    uint32_t groupCountZ = (VOXEL_RESOLUTION + 7) / 8;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

    VkImageMemoryBarrier2 barrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier2.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.image = image;                 // SDFPass target
    barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1 };

    VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier2;

    vkCmdPipelineBarrier2(commandBuffer, &dep);
}

void VoxelizerPass::DebugCompute(uint32_t currentFrame)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VkDeviceSize bufferSize = sizeof(Voxel) * VOXEL_COUNT;

    VkFence& fence = app->computeFences[currentFrame];
    if (fence != VK_NULL_HANDLE) {
        if (vkGetFenceStatus(app->_logicalDevice, fence) == VK_NOT_READY)
        {
            return;
        }
    }

    // Command buffer for async transferdoe
    VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

    // Record copy command, add copy to command buffer!
    VkBuffer srcBuffer = shaderStorageBuffers[currentFrame];
    VkBufferCopy copyRegion{};
    copyRegion.size = sizeof(Voxel) * VOXEL_COUNT;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, readbackBuffers[currentFrame], 1, &copyRegion);

    app->EndSingleTimeCommandsAsync(currentFrame, commandBuffer, [=]() {
        Voxel* mappedVoxels = nullptr;

        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = readbackBufferMemories[currentFrame];
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;

        vkInvalidateMappedMemoryRanges(app->_logicalDevice, 1, &range);
        vkMapMemory(app->_logicalDevice, readbackBufferMemories[currentFrame], 0, bufferSize, 0, reinterpret_cast<void**>(&mappedVoxels));
        std::memcpy(frameReadbackData[currentFrame].data(), mappedVoxels, sizeof(Voxel) * VOXEL_COUNT);
        vkUnmapMemory(app->_logicalDevice, readbackBufferMemories[currentFrame]);
        });


    for (size_t i = 0; i < std::min((size_t)10, (size_t)VOXEL_COUNT); ++i) {
        std::cout << "Voxel: " << i << "Position: " << frameReadbackData[currentFrame][i].positionDistance.x << ", " << frameReadbackData[currentFrame][i].positionDistance.y << ", " << frameReadbackData[currentFrame][i].positionDistance.z << std::endl;
        std::cout << "Distance: " << frameReadbackData[currentFrame][i].positionDistance.w << std::endl;
        std::cout << "Normal: " << frameReadbackData[currentFrame][i].normalDensity.x << ", " << frameReadbackData[currentFrame][i].normalDensity.y << ", " << frameReadbackData[currentFrame][i].normalDensity.z << std::endl;
        std::cout << "Density: " << frameReadbackData[currentFrame][i].normalDensity.w << std::endl;

    }
}