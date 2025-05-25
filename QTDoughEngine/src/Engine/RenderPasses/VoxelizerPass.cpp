#include "VoxelizerPass.h"
#include <random>

VoxelizerPass* VoxelizerPass::instance = nullptr;

VoxelizerPass::~VoxelizerPass() {
    PassName = "VoxelizerPass";
}

VoxelizerPass::VoxelizerPass() {
    VOXEL_COUNTL1 = VOXEL_RESOLUTIONL1 * VOXEL_RESOLUTIONL1 * VOXEL_RESOLUTIONL1;
    VOXEL_COUNTL2 = VOXEL_RESOLUTIONL2 * VOXEL_RESOLUTIONL2 * VOXEL_RESOLUTIONL2;
    VOXEL_COUNTL3 = VOXEL_RESOLUTIONL3 * VOXEL_RESOLUTIONL3 * VOXEL_RESOLUTIONL3;
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
    /*
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
    */
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

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float);

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
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pushConstantRange;

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

    std::cout << "Memory of voxels in L1: " << sizeof(Voxel) * VOXEL_COUNTL1 / 1024.0f / 1024.0f << " MB" << std::endl;
    std::cout << "Memory of voxels in L2: " << sizeof(Voxel) * VOXEL_COUNTL2 / 1024.0f / 1024.0f << " MB" << std::endl;
    std::cout << "Memory of voxels in L3: " << sizeof(Voxel) * VOXEL_COUNTL3 / 1024.0f / 1024.0f << " MB" << std::endl;
    std::cout << "Size of voxel: " << sizeof(Voxel) << " bytes" << std::endl;

    readbackBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    readbackBufferMemories.resize(app->MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; ++i) {
        app->CreateBuffer(
            sizeof(Voxel) * VOXEL_COUNTL3, //L3 is the readable one.
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            readbackBuffers[i],
            readbackBufferMemories[i]
        );
    }
    frameReadbackData.resize(app->MAX_FRAMES_IN_FLIGHT);
    for (auto& frame : frameReadbackData)
        frame.resize(VOXEL_COUNTL3);

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


        std::array<VkWriteDescriptorSet, 9> descriptorWrites{};
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

        //L1
        VkDescriptorBufferInfo voxelL1BufferInfoLastFrame{};
        voxelL1BufferInfoLastFrame.buffer = voxelL1StorageBuffers[(i - 1) % app->MAX_FRAMES_IN_FLIGHT];
        voxelL1BufferInfoLastFrame.offset = 0;
        voxelL1BufferInfoLastFrame.range = sizeof(Voxel) * VOXEL_COUNTL1;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = computeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &voxelL1BufferInfoLastFrame;

        VkDescriptorBufferInfo voxelL1BufferInfoCurrentFrame{};
        voxelL1BufferInfoCurrentFrame.buffer = voxelL1StorageBuffers[i];
        voxelL1BufferInfoCurrentFrame.offset = 0;
        voxelL1BufferInfoCurrentFrame.range = sizeof(Voxel) * VOXEL_COUNTL1;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = computeDescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &voxelL1BufferInfoCurrentFrame;

        //L2
        VkDescriptorBufferInfo voxel21BufferInfoLastFrame{};
        voxel21BufferInfoLastFrame.buffer = voxelL2StorageBuffers[(i - 1) % app->MAX_FRAMES_IN_FLIGHT];
        voxel21BufferInfoLastFrame.offset = 0;
        voxel21BufferInfoLastFrame.range = sizeof(Voxel) * VOXEL_COUNTL2;

        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = computeDescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &voxel21BufferInfoLastFrame;

        VkDescriptorBufferInfo voxelL2BufferInfoCurrentFrame{};
        voxelL2BufferInfoCurrentFrame.buffer = voxelL2StorageBuffers[i];
        voxelL2BufferInfoCurrentFrame.offset = 0;
        voxelL2BufferInfoCurrentFrame.range = sizeof(Voxel) * VOXEL_COUNTL2;

        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = computeDescriptorSets[i];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pBufferInfo = &voxelL2BufferInfoCurrentFrame;

        //L3
        VkDescriptorBufferInfo voxelL3BufferInfoLastFrame{};
        voxelL3BufferInfoLastFrame.buffer = voxelL3StorageBuffers[(i - 1) % app->MAX_FRAMES_IN_FLIGHT];
        voxelL3BufferInfoLastFrame.offset = 0;
        voxelL3BufferInfoLastFrame.range = sizeof(Voxel) * VOXEL_COUNTL3;

        descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[6].dstSet = computeDescriptorSets[i];
        descriptorWrites[6].dstBinding = 6;
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pBufferInfo = &voxelL3BufferInfoLastFrame;

        VkDescriptorBufferInfo voxelL3BufferInfoCurrentFrame{};
        voxelL3BufferInfoCurrentFrame.buffer = voxelL3StorageBuffers[i];
        voxelL3BufferInfoCurrentFrame.offset = 0;
        voxelL3BufferInfoCurrentFrame.range = sizeof(Voxel) * VOXEL_COUNTL3;

        descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[7].dstSet = computeDescriptorSets[i];
        descriptorWrites[7].dstBinding = 7;
        descriptorWrites[7].dstArrayElement = 0;
        descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[7].descriptorCount = 1;
        descriptorWrites[7].pBufferInfo = &voxelL3BufferInfoCurrentFrame;

        //Vertex info.
        VkDescriptorBufferInfo vInfo{};
        vInfo.buffer = vertexBuffer;
        vInfo.offset = 0;
        vInfo.range = VK_WHOLE_SIZE;

        descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[8].dstSet = computeDescriptorSets[i];
        descriptorWrites[8].dstBinding = 8;
        descriptorWrites[8].descriptorCount = 1;
        descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[8].pBufferInfo = &vInfo;

        vkUpdateDescriptorSets(app->_logicalDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    std::cout << "Created descriptor sets" << std::endl;
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
    VkDescriptorSetLayoutBinding voxelL1Binding{};
    voxelL1Binding.binding = 2;
    voxelL1Binding.descriptorCount = 1;
    voxelL1Binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    voxelL1Binding.pImmutableSamplers = nullptr;
    voxelL1Binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Fourth binding is for storage buffer. General purpose.
    VkDescriptorSetLayoutBinding voxelL1Binding2{};
    voxelL1Binding2.binding = 3;
    voxelL1Binding2.descriptorCount = 1;
    voxelL1Binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    voxelL1Binding2.pImmutableSamplers = nullptr;
    voxelL1Binding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Do L2 and L3
    //Fifth binding for storage buffer.
    VkDescriptorSetLayoutBinding voxelL2Binding{};
    voxelL2Binding.binding = 4;
    voxelL2Binding.descriptorCount = 1;
    voxelL2Binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    voxelL2Binding.pImmutableSamplers = nullptr;
    voxelL2Binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding voxelL2Binding2{};
    voxelL2Binding2.binding = 5;
    voxelL2Binding2.descriptorCount = 1;
    voxelL2Binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    voxelL2Binding2.pImmutableSamplers = nullptr;
    voxelL2Binding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding voxelL3Binding{};
    voxelL3Binding.binding = 6;
    voxelL3Binding.descriptorCount = 1;
    voxelL3Binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    voxelL3Binding.pImmutableSamplers = nullptr;
    voxelL3Binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding voxelL3Binding2{};
    voxelL3Binding2.binding = 7;
    voxelL3Binding2.descriptorCount = 1;
    voxelL3Binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    voxelL3Binding2.pImmutableSamplers = nullptr;
    voxelL3Binding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;


    //Fith binding for vertex buffer.
    VkDescriptorSetLayoutBinding vertexBinding{};
    vertexBinding.binding = 8;
    vertexBinding.descriptorCount = 1;
    vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBinding.pImmutableSamplers = nullptr;
    vertexBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Bind the buffers we specified.
    std::array<VkDescriptorSetLayoutBinding, 9> bindings = { uboLayoutBinding, intArrayLayoutBinding, voxelL1Binding, voxelL1Binding2, voxelL2Binding, voxelL2Binding2, voxelL3Binding, voxelL3Binding2, vertexBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(app->_logicalDevice, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
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

        
        float density = SCENE_BOUNDSL1 / (float)VOXEL_RESOLUTIONL1;
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


}

void VoxelizerPass::CreateShaderStorageBuffers()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    std::cout << "Creating Shader Storage Buffers " << PassName << std::endl;

    //Create the triangle soup first.
    CreateTriangleSoup();
    // Initial data. This should create the voxel scene grid.
    voxelsL1.resize(VOXEL_COUNTL1);
    voxelsL2.resize(VOXEL_COUNTL2);
    voxelsL3.resize(VOXEL_COUNTL3);

    VkDeviceSize bufferSizeL1 = sizeof(Voxel) * VOXEL_COUNTL1;
    VkDeviceSize bufferSizeL2 = sizeof(Voxel) * VOXEL_COUNTL2;
    VkDeviceSize bufferSizeL3 = sizeof(Voxel) * VOXEL_COUNTL3;

    // Create a staging buffer used to upload data to the gpu
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app->CreateBuffer(bufferSizeL1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(app->_logicalDevice, stagingBufferMemory, 0, bufferSizeL1, 0, &data);
    std::memcpy(data, voxelsL1.data(), voxelsL1.size() * sizeof(Voxel));
    vkUnmapMemory(app->_logicalDevice, stagingBufferMemory);

    voxelL1StorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    voxelL1StorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);

    // Copy initial data to all storage buffers
    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        app->CreateBuffer(bufferSizeL1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelL1StorageBuffers[i], voxelL1StorageBuffersMemory[i]);
        app->CopyBuffer(stagingBuffer, voxelL1StorageBuffers[i], bufferSizeL1);
    }

    vkDestroyBuffer(app->_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBufferMemory, nullptr);

    std::cout << "Voxel L1 Storage Buffers created" << std::endl;

    //Do this for next level mips.
    // Create a staging buffer used to upload data to the gpu
    VkBuffer stagingBuffer2;
    VkDeviceMemory stagingBufferMemory2;
    app->CreateBuffer(bufferSizeL2, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer2, stagingBufferMemory2);

    void* data2;
    vkMapMemory(app->_logicalDevice, stagingBufferMemory2, 0, bufferSizeL2, 0, &data2);
    std::memcpy(data2, voxelsL2.data(), voxelsL2.size() * sizeof(Voxel));
    vkUnmapMemory(app->_logicalDevice, stagingBufferMemory2);
    voxelL2StorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    voxelL2StorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);
    // Copy initial data to all storage buffers
    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
		app->CreateBuffer(bufferSizeL2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelL2StorageBuffers[i], voxelL2StorageBuffersMemory[i]);
		app->CopyBuffer(stagingBuffer2, voxelL2StorageBuffers[i], bufferSizeL2);
	}

    vkDestroyBuffer(app->_logicalDevice, stagingBuffer2, nullptr);
	vkFreeMemory(app->_logicalDevice, stagingBufferMemory2, nullptr);

	//Do this for next level mips.
	// Create a staging buffer used to upload data to the gpu
	VkBuffer stagingBuffer3;
	VkDeviceMemory stagingBufferMemory3;
	app->CreateBuffer(bufferSizeL3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer3, stagingBufferMemory3);

	void* data3;
	vkMapMemory(app->_logicalDevice, stagingBufferMemory3, 0, bufferSizeL3, 0, &data3);
	std::memcpy(data3, voxelsL3.data(), voxelsL3.size() * sizeof(Voxel));
	vkUnmapMemory(app->_logicalDevice, stagingBufferMemory3);
	voxelL3StorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
	voxelL3StorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);
	// Copy initial data to all storage buffers
	for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
		app->CreateBuffer(bufferSizeL3, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelL3StorageBuffers[i], voxelL3StorageBuffersMemory[i]);
		app->CopyBuffer(stagingBuffer3, voxelL3StorageBuffers[i], bufferSizeL3);
	}

	vkDestroyBuffer(app->_logicalDevice, stagingBuffer3, nullptr);
	vkFreeMemory(app->_logicalDevice, stagingBufferMemory3, nullptr);

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

    DispatchLOD(commandBuffer, currentFrame, 1);
    DispatchLOD(commandBuffer, currentFrame, 2);
    DispatchLOD(commandBuffer, currentFrame, 3);


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

void VoxelizerPass::DispatchLOD(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

    float lod = static_cast<float>(lodLevel);
    vkCmdPushConstants(
        commandBuffer,
        computePipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(float),
        &lod
    );

    VkDescriptorSet sets[] = {
        app->globalDescriptorSets[currentFrame],
        computeDescriptorSets[currentFrame]
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        computePipelineLayout,
        0, 2, sets,
        0, nullptr);

    // Each LOD uses a different resolution
    uint32_t res = (lodLevel == 1) ? VOXEL_RESOLUTIONL1 :
        (lodLevel == 2) ? VOXEL_RESOLUTIONL2 :
        VOXEL_RESOLUTIONL3;

    uint32_t groupCountX = (res + 7) / 8;
    uint32_t groupCountY = (res + 7) / 8;
    uint32_t groupCountZ = (res + 7) / 8;

    /*
    // Add debug label for Nsight
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = (lodLevel == 1) ? "LOD1 Dispatch" :
        (lodLevel == 2) ? "LOD2 Dispatch" : "LOD3 Dispatch";
    vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
    */


    
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

    //vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}


void VoxelizerPass::IsOccupiedByVoxel()
{
    /*
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
    */
}

void VoxelizerPass::DebugCompute(uint32_t currentFrame)
{
    /*
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
    */
}