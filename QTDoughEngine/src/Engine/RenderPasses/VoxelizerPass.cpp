#include "VoxelizerPass.h"
#include <random>

VoxelizerPass* VoxelizerPass::instance = nullptr;
// --- keep ping local to this translation unit ---
static bool ping = false;


VoxelizerPass::~VoxelizerPass() {
    PassName = "VoxelizerPass";
}

VoxelizerPass::VoxelizerPass() {
    VOXEL_COUNTL1 = VOXEL_RESOLUTIONL1 * VOXEL_RESOLUTIONL1 * VOXEL_RESOLUTIONL1;
    VOXEL_COUNTL2 = VOXEL_RESOLUTIONL2 * VOXEL_RESOLUTIONL2 * VOXEL_RESOLUTIONL2;
    VOXEL_COUNTL3 = VOXEL_RESOLUTIONL3 * VOXEL_RESOLUTIONL3 * VOXEL_RESOLUTIONL3;
    TILE_COUNTL1 = VOXEL_RESOLUTIONL1 / TILE_SIZE;
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

void VoxelizerPass::CreateComputePipelineName(std::string shaderPass, VkPipeline& rcomputePipeline, VkPipelineLayout& rcomputePipelineLayout) {

    QTDoughApplication* app = QTDoughApplication::instance;

    auto computeShaderCode = readFile("src/shaders/" + shaderPass + ".spv");
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

    if (vkCreatePipelineLayout(app->_logicalDevice, &pli, nullptr, &rcomputePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = rcomputePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(app->_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rcomputePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(app->_logicalDevice, computeShaderModule, nullptr);
}

void VoxelizerPass::CreateComputePipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    CreateComputePipelineName("voxelizer", voxelizeComputePipeline, voxelizeComputePipelineLayout);
    CreateComputePipelineName("tilesdf", tileGenerationComputePipeline, tileGenerationComputePipelineLayout);
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

void VoxelizerPass::CreateDescriptorPool()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    const uint32_t kFrameSets = app->MAX_FRAMES_IN_FLIGHT;
    const uint32_t kSweepSets = 2;                       // ping + pong
    const uint32_t kTotalSets = kFrameSets + kSweepSets;

    VkDescriptorPoolSize poolSizes[2]{};

    // one UBO per set (binding 0)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = kTotalSets * 1;

    // twelve storage buffers per set (bindings 1-11)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = kTotalSets * 12;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = kTotalSets;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // handy for hot-reloads

    VK_CHECK(vkCreateDescriptorPool(app->_logicalDevice, &poolInfo, nullptr, &descriptorPool));
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



    //Create two buffer per frame:

    for (int pp = 0; pp < 2; ++pp)
    {
        app->CreateBuffer(bufferSizeL1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            voxL1PingPong[pp], voxL1PingPongMem[pp]);
        app->CopyBuffer(stagingBuffer, voxL1PingPong[pp], bufferSizeL1);

        app->CreateBuffer(bufferSizeL2,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            voxL2PingPong[pp], voxL2PingPongMem[pp]);
        app->CopyBuffer(stagingBuffer2, voxL2PingPong[pp], bufferSizeL2);

        app->CreateBuffer(bufferSizeL3,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            voxL3PingPong[pp], voxL3PingPongMem[pp]);
        app->CopyBuffer(stagingBuffer3, voxL3PingPong[pp], bufferSizeL3);
    }



    vkDestroyBuffer(app->_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBufferMemory, nullptr);
    vkDestroyBuffer(app->_logicalDevice, stagingBuffer2, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBufferMemory2, nullptr);
    vkDestroyBuffer(app->_logicalDevice, stagingBuffer3, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBufferMemory3, nullptr);

    //Create brushes buffers
    app->CreateBuffer(
        sizeof(Brush) * brushes.size(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        brushesStorageBuffers,
        brushesStorageMemory
    );

    // Copy initial data to the brushes storage buffer
    VkBuffer stagingBrushBuffer;
    VkDeviceMemory stagingBrushMemory;
    app->CreateBuffer(
        sizeof(Brush) * brushes.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBrushBuffer, stagingBrushMemory
    );

    void* brushData;
    vkMapMemory(app->_logicalDevice, stagingBrushMemory, 0, sizeof(Brush) * brushes.size(), 0, &brushData);
    memcpy(brushData, brushes.data(), sizeof(Brush) * brushes.size());
    vkUnmapMemory(app->_logicalDevice, stagingBrushMemory);

    app->CopyBuffer(stagingBrushBuffer, brushesStorageBuffers, sizeof(Brush) * brushes.size());

    vkDestroyBuffer(app->_logicalDevice, stagingBrushBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBrushMemory, nullptr);

    //Create Tiles Storage Buffers.
    VkBuffer stagingBrushIndicesBuffer;
    VkDeviceMemory stagingBrushIndicesMemory;

    uint32_t brushListSize = (VOXEL_RESOLUTIONL1 / TILE_SIZE) * (VOXEL_RESOLUTIONL1 / TILE_SIZE) * (VOXEL_RESOLUTIONL1 / TILE_SIZE) * TILE_MAX_BRUSHES;

    BrushesIndices.resize(brushListSize, 4294967295); // Initialize with max uint32_t value
    app->CreateBuffer(
        sizeof(uint32_t) * brushListSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBrushIndicesBuffer, stagingBrushIndicesMemory
    );

    void* brushIndicesData;

    vkMapMemory(app->_logicalDevice, stagingBrushIndicesMemory, 0, sizeof(uint32_t) * brushListSize, 0, &brushIndicesData);
    memcpy(brushIndicesData, BrushesIndices.data(), sizeof(uint32_t) * brushListSize);
    vkUnmapMemory(app->_logicalDevice, stagingBrushIndicesMemory);

    app->CreateBuffer(
        sizeof(uint32_t) * brushListSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        brushIndicesStorageBuffers, brushIndicesStorageMemory
    );

    app->CopyBuffer(stagingBrushIndicesBuffer, brushIndicesStorageBuffers, sizeof(uint32_t) * brushListSize);

    vkDestroyBuffer(app->_logicalDevice, stagingBrushIndicesBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBrushIndicesMemory, nullptr);

    //Tile Brush Counts.
    VkBuffer stagingBrushCountsBuffer;
    VkDeviceMemory stagingBrushCountsMemory;
    uint32_t brushCountsSize = (VOXEL_RESOLUTIONL1 / TILE_SIZE) * (VOXEL_RESOLUTIONL1 / TILE_SIZE) * (VOXEL_RESOLUTIONL1 / TILE_SIZE);
    TilesBrushCounts.resize(brushCountsSize, 0); // Initialize with 0

    app->CreateBuffer(
        sizeof(uint32_t) * brushCountsSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBrushCountsBuffer, stagingBrushCountsMemory
    );

    void* brushCountsData;
    vkMapMemory(app->_logicalDevice, stagingBrushCountsMemory, 0, sizeof(uint32_t) * brushCountsSize, 0, &brushCountsData);
    memcpy(brushCountsData, TilesBrushCounts.data(), sizeof(uint32_t) * brushCountsSize);
    vkUnmapMemory(app->_logicalDevice, stagingBrushCountsMemory);

    app->CreateBuffer(
        sizeof(uint32_t) * brushCountsSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tileBrushCountStorageBuffers, tileBrushCountStorageMemory
    );

    app->CopyBuffer(stagingBrushCountsBuffer, tileBrushCountStorageBuffers, sizeof(uint32_t) * brushCountsSize);

    vkDestroyBuffer(app->_logicalDevice, stagingBrushCountsBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBrushCountsMemory, nullptr);

    //Create the storage buffers for L2 and L3.
    voxelL2StorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    voxelL2StorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);
    voxelL3StorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    voxelL3StorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);

    std::cout << "VoxelizerPass: " << PassName << " - ";


    //Create Particles.

    uint32_t particleBufferSize = sizeof(Particle) * PARTICLE_COUNT;

    //Initialize particles data.
    particles.resize(PARTICLE_COUNT);
    for (uint32_t i = 0; i < PARTICLE_COUNT; ++i) {
        particles[i].position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        particles[i].velocity = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    VkBuffer pStagingBuffer;
    VkDeviceMemory pStagingBufferMemory;
    app->CreateBuffer(particleBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, pStagingBuffer, pStagingBufferMemory);

    void* pdata;
    vkMapMemory(app->_logicalDevice, pStagingBufferMemory, 0, particleBufferSize, 0, &pdata);
    std::memcpy(pdata, particles.data(), particles.size() * sizeof(Particle));
    vkUnmapMemory(app->_logicalDevice, pStagingBufferMemory);

    particlesStorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    particlesStorageMemory.resize(app->MAX_FRAMES_IN_FLIGHT);

    // Copy initial data to all storage buffers
    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        app->CreateBuffer(particleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, particlesStorageBuffers[i], particlesStorageMemory[i]);
        app->CopyBuffer(pStagingBuffer, particlesStorageBuffers[i], particleBufferSize);
    }

    std::cout << "Shader Storage Buffers created" << std::endl;
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

    // Brush buffer shared across all frames
    VkDescriptorBufferInfo brushBufferInfo{};
    brushBufferInfo.buffer = brushesStorageBuffers;
    brushBufferInfo.offset = 0;
    brushBufferInfo.range = VK_WHOLE_SIZE;

    //Tiles Brushes Indices
    VkDescriptorBufferInfo brushIndicesBufferInfo{};
    brushIndicesBufferInfo.buffer = brushIndicesStorageBuffers;
    brushIndicesBufferInfo.offset = 0;
    brushIndicesBufferInfo.range = VK_WHOLE_SIZE;

    //Tiles brush counts.
    VkDescriptorBufferInfo tileBrushCountBufferInfo{};
    tileBrushCountBufferInfo.buffer = tileBrushCountStorageBuffers;
    tileBrushCountBufferInfo.offset = 0;
    tileBrushCountBufferInfo.range = VK_WHOLE_SIZE;




    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = _uniformBuffers[i];
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo intArrayBufferInfo{};
        intArrayBufferInfo.buffer = intArrayBuffer;
        intArrayBufferInfo.offset = 0;
        intArrayBufferInfo.range = VK_WHOLE_SIZE;


        std::array<VkWriteDescriptorSet, 14> descriptorWrites{};
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

        descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[9].dstSet = computeDescriptorSets[i];
        descriptorWrites[9].dstBinding = 9;
        descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[9].descriptorCount = 1;
        descriptorWrites[9].pBufferInfo = &brushBufferInfo;

        //Brush indices
        descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[10].dstSet = computeDescriptorSets[i];
        descriptorWrites[10].dstBinding = 10;
        descriptorWrites[10].dstArrayElement = 0;
        descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[10].descriptorCount = 1;
        descriptorWrites[10].pBufferInfo = &brushIndicesBufferInfo;

        //Tile brush counts
        descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[11].dstSet = computeDescriptorSets[i];
        descriptorWrites[11].dstBinding = 11;
        descriptorWrites[11].dstArrayElement = 0;
        descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[11].descriptorCount = 1;
        descriptorWrites[11].pBufferInfo = &tileBrushCountBufferInfo;

        //Particles.

        VkDescriptorBufferInfo particleBufferInfoLastFrame{};
        particleBufferInfoLastFrame.buffer = particlesStorageBuffers[(i - 1) % app->MAX_FRAMES_IN_FLIGHT];
        particleBufferInfoLastFrame.offset = 0;
        particleBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[12].dstSet = computeDescriptorSets[i];
        descriptorWrites[12].dstBinding = 12;
        descriptorWrites[12].dstArrayElement = 0;
        descriptorWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[12].descriptorCount = 1;
        descriptorWrites[12].pBufferInfo = &particleBufferInfoLastFrame;

        VkDescriptorBufferInfo particleBufferInfoCurrentFrame{};
        particleBufferInfoCurrentFrame.buffer = particlesStorageBuffers[i];
        particleBufferInfoCurrentFrame.offset = 0;
        particleBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[13].dstSet = computeDescriptorSets[i];
        descriptorWrites[13].dstBinding = 13;
        descriptorWrites[13].dstArrayElement = 0;
        descriptorWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[13].descriptorCount = 1;
        descriptorWrites[13].pBufferInfo = &particleBufferInfoCurrentFrame;


        vkUpdateDescriptorSets(app->_logicalDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    CreateSweepDescriptorSets();

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

    VkDescriptorSetLayoutBinding brushBinding{};
    brushBinding.binding = 9;
    brushBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    brushBinding.descriptorCount = 1;
    brushBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Brush indices binding
    VkDescriptorSetLayoutBinding brushIndicesBinding{};
    brushIndicesBinding.binding = 10;
    brushIndicesBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    brushIndicesBinding.descriptorCount = 1;
    brushIndicesBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Tile brush counts binding
    VkDescriptorSetLayoutBinding tileBrushCountBinding{};
    tileBrushCountBinding.binding = 11;
    tileBrushCountBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    tileBrushCountBinding.descriptorCount = 1;
    tileBrushCountBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding particleBinding1{};
    particleBinding1.binding = 12;
    particleBinding1.descriptorCount = 1;
    particleBinding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    particleBinding1.pImmutableSamplers = nullptr;
    particleBinding1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding particleBinding2{};
    particleBinding2.binding = 13;
    particleBinding2.descriptorCount = 1;
    particleBinding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    particleBinding2.pImmutableSamplers = nullptr;
    particleBinding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Bind the buffers we specified.
    std::array<VkDescriptorSetLayoutBinding, 14> bindings = { uboLayoutBinding, intArrayLayoutBinding, voxelL1Binding, voxelL1Binding2, voxelL2Binding, voxelL2Binding2, voxelL3Binding, voxelL3Binding2, vertexBinding, brushBinding, brushIndicesBinding, tileBrushCountBinding, particleBinding1, particleBinding2 };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(app->_logicalDevice, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }

}

float Read3DTransformedDebug(const glm::mat4& modelMatrix, int resolution, const glm::vec3& worldPos)
{
    // Inverse transform to get local position
    glm::mat4 invModel = glm::inverse(modelMatrix);
    glm::vec4 local = invModel * glm::vec4(worldPos, 1.0f);
    glm::vec3 localPos = glm::vec3(local);

    // Map from [-4, 4] to [0, resolution)
    glm::vec3 uvw = (localPos + glm::vec3(1.0f)) * 0.5f;
    glm::ivec3 voxelCoord = glm::ivec3(uvw * static_cast<float>(resolution));

    // Debug print
    std::cout << "World Pos: " << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << "\n";
    std::cout << "Local Pos: " << localPos.x << ", " << localPos.y << ", " << localPos.z << "\n";
    std::cout << "Voxel Coord: " << voxelCoord.x << ", " << voxelCoord.y << ", " << voxelCoord.z << "\n";

    // Bounds check
    if (voxelCoord.x < 0 || voxelCoord.y < 0 || voxelCoord.z < 0 ||
        voxelCoord.x >= resolution || voxelCoord.y >= resolution || voxelCoord.z >= resolution)
    {
        std::cout << "Out of bounds — returning 1.0f\n";
        return 1.0f; // Outside brush volume
    }

    std::cout << "Within bounds — would sample texture here\n";

    // NOTE: This is a debug function, so we return dummy 0
    return 0.0f; // Replace with actual texture sampling if you want
}

void VoxelizerPass::CreateBrushes()
{
    int vertexOffset = 0;
    for (int i = 0; i < renderingObjects.size(); i++)
    {
        UnigmaRenderingObject* obj = renderingObjects[i];
        Brush brush;
        brush.type = 0; //Mesh type
        brush.vertexCount = obj->_renderer.vertices.size();

        //std::cout << "Creating brush for object: " << i << " with vertex count: " << brush.vertexCount << std::endl;
        brush.vertexOffset = vertexOffset;
        brush.textureID = i;

        //Set the resolution for the brush.
        brush.resolution = VOXEL_RESOLUTIONL2; //Set to L1 for now. Later on this is read from the object.

        //Create the model matrix for the brush.
        //obj->_transform.position = glm::vec3(0.0f, 0.0f, 0.0f); // Set to origin for now
        brush.model = obj->_transform.GetModelMatrixBrush();

        /*
        glm::mat4x4 inverseModel = glm::inverse(brush.model);
        std::cout << "Model Matrix:\n";
        for (int row = 0; row < 4; ++row) {
            std::cout << "[ ";
            for (int col = 0; col < 4; ++col) {
                std::cout << brush.model[col][row]; // GLM is column-major
                if (col < 3) std::cout << ", ";
            }
            std::cout << " ]\n";
        }
        std::cout << "Inverse Model Matrix:\n";
        for (int row = 0; row < 4; ++row) {
            std::cout << "[ ";
            for (int col = 0; col < 4; ++col) {
                std::cout << inverseModel[col][row]; // GLM is column-major
                if (col < 3) std::cout << ", ";
            }
            std::cout << " ]\n";
        }
        
        //Print out all vertices positions.
        std::cout << "Vertices positions:\n";
        for (const auto& vertex : obj->_renderer.vertices)
		{
			glm::vec3 pos = glm::vec3(vertex.pos);
			std::cout << "Vertex Position: " << pos.x << ", " << pos.y << ", " << pos.z << "\n";
		}
        
        Read3DTransformedDebug(brush.model, brush.resolution, glm::vec3(0.0f, 0.0f, 0.0f));
        */

        //Add the brush to the list.
        brushes.push_back(brush);

        vertexOffset += brush.vertexCount;

    }
}

void VoxelizerPass::Create3DTextures()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    VkFormat sdfFormat = app->FindSupportedFormat(
        {
            VK_FORMAT_R32_SFLOAT
        },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
    );

    for (int i = 0; i < brushes.size(); i++)
    {
        Brush& brush = brushes[i];

        if(brush.type != 0) {
			continue;
		}

        Unigma3DTexture brushTexture = Unigma3DTexture(brush.resolution, brush.resolution, brush.resolution);
		app->CreateImages3D(brushTexture.WIDTH, brushTexture.HEIGHT, brushTexture.DEPTH,
			sdfFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			brushTexture.u_image, brushTexture.u_imageMemory);

		brushTexture.u_imageView = app->Create3DImageView(
			brushTexture.u_image,
			sdfFormat,
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		// Transition the 3D image layout to GENERAL for compute write
		VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = brushTexture.u_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 5;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr,
			1, &barrier);

		app->EndSingleTimeCommands(commandBuffer);

		app->textures3D.insert({ "brush_" + std::to_string(i), brushTexture });
    }
}

void VoxelizerPass::CreateImages() {
    QTDoughApplication* app = QTDoughApplication::instance;

    //Get the images path tied to this material.
    for (int i = 0; i < material.textures.size(); i++) {
        app->LoadTexture(material.textures[i].TEXTURE_PATH);
    }

    //Load all textures for all objects within this pass.
    for (int i = 0; i < renderingObjects.size(); i++)
    {
        for (int j = 0; j < renderingObjects[i]->_material.textures.size(); j++)
        {
            app->LoadTexture(renderingObjects[i]->_material.textures[j].TEXTURE_PATH);
        }
    }

    VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;//app->_swapChainImageFormat;

    // Create the offscreen image
    app->CreateImage(
        app->swapChainExtent.width,
        app->swapChainExtent.height,
        imageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image,
        imageMemory
    );

    CreateBrushes();
    Create3DTextures();

    VkCommandBuffer commandBuffer = app->BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 5;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    app->EndSingleTimeCommands(commandBuffer);

    // Create the image view for the offscreen image
    imageView = app->CreateImageView(image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    commandBuffer = app->BeginSingleTimeCommands();

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    app->EndSingleTimeCommands(commandBuffer);

    // Store the offscreen texture for future references
    UnigmaTexture offscreenTexture;
    offscreenTexture.u_image = image;
    offscreenTexture.u_imageView = imageView;
    offscreenTexture.u_imageMemory = imageMemory;
    offscreenTexture.TEXTURE_PATH = PassName;

    // Use a unique key for the offscreen image
    std::string textureKey = PassName;
    app->textures.insert({ textureKey, offscreenTexture });
}

void VoxelizerPass::UpdateBrushesGPU(VkCommandBuffer commandBuffer)
{
    // Update CPU-side brushes first
    for (size_t i = 0; i < renderingObjects.size(); ++i)
    {
        brushes[i].model = renderingObjects[i]->_transform.GetModelMatrixBrush();
    }

    // Use vkCmdUpdateBuffer to update GPU buffer
    for (size_t i = 0; i < brushes.size(); ++i)
    {
        VkDeviceSize offset = sizeof(Brush) * i + offsetof(Brush, model);

        vkCmdUpdateBuffer(
            commandBuffer,
            brushesStorageBuffers,
            offset,
            sizeof(glm::mat4),
            &brushes[i].model
        );
    }

    // Memory barrier after all updates
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr
    );
}

void VoxelizerPass::UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) {

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

    //Update brushes.



    /*
    vertices.clear();


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
    */


}

void VoxelizerPass::CreateSweepDescriptorSets()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    /* --- allocate the two descriptor sets --------------------------------- */
    VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = 2;
    VkDescriptorSetLayout layouts[2] = { computeDescriptorSetLayout,
                                         computeDescriptorSetLayout };
    ai.pSetLayouts = layouts;
    VK_CHECK(vkAllocateDescriptorSets(app->_logicalDevice, &ai, sweepSets));

    /* build a buffer-info for binding 1 (texture-ID array) ------------------ */
    VkDescriptorBufferInfo texIDInfo{};
    texIDInfo.buffer = intArrayBuffer;   // member variable created earlier
    texIDInfo.offset = 0;
    texIDInfo.range = VK_WHOLE_SIZE;

    /* helper to fill one of the two sets ----------------------------------- */
    auto fill = [&](uint32_t setIdx, bool pingIsRead)
        {
            /* ping-pong voxel buffers */
            VkDescriptorBufferInfo r1{ voxL1PingPong[pingIsRead ? 0 : 1], 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo w1{ voxL1PingPong[pingIsRead ? 1 : 0], 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo r2{ voxL2PingPong[pingIsRead ? 0 : 1], 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo w2{ voxL2PingPong[pingIsRead ? 1 : 0], 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo r3{ voxL3PingPong[pingIsRead ? 0 : 1], 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo w3{ voxL3PingPong[pingIsRead ? 1 : 0], 0, VK_WHOLE_SIZE };

            /* the other bindings we keep identical to earlier sets */
            VkDescriptorBufferInfo uboInfo{ _uniformBuffers[0],           0, sizeof(UniformBufferObject) };
            VkDescriptorBufferInfo vtxInfo{ vertexBuffer,                 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo brushInfo{ brushesStorageBuffers,        0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo idxInfo{ brushIndicesStorageBuffers,   0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo cntInfo{ tileBrushCountStorageBuffers, 0, VK_WHOLE_SIZE };

            VkWriteDescriptorSet ws[12]{};
            for (int i = 0; i < 12; ++i)
            {
                ws[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                ws[i].dstSet = sweepSets[setIdx];
                ws[i].descriptorCount = 1;
                ws[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // overwritten for UBO below
            }

            /* binding 0 : UBO */
            ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ws[0].dstBinding = 0;  ws[0].pBufferInfo = &uboInfo;

            /* binding 1 : texture-ID array */
            ws[1].dstBinding = 1;  ws[1].pBufferInfo = &texIDInfo;

            /* bindings 2-7 : ping-pong voxel buffers */
            ws[2].dstBinding = 2;  ws[2].pBufferInfo = &r1;
            ws[3].dstBinding = 3;  ws[3].pBufferInfo = &w1;
            ws[4].dstBinding = 4;  ws[4].pBufferInfo = &r2;
            ws[5].dstBinding = 5;  ws[5].pBufferInfo = &w2;
            ws[6].dstBinding = 6;  ws[6].pBufferInfo = &r3;
            ws[7].dstBinding = 7;  ws[7].pBufferInfo = &w3;

            /* bindings 8-11 : vertex + brush data */
            ws[8].dstBinding = 8;  ws[8].pBufferInfo = &vtxInfo;
            ws[9].dstBinding = 9;  ws[9].pBufferInfo = &brushInfo;
            ws[10].dstBinding = 10; ws[10].pBufferInfo = &idxInfo;
            ws[11].dstBinding = 11; ws[11].pBufferInfo = &cntInfo;

            vkUpdateDescriptorSets(app->_logicalDevice,
                static_cast<uint32_t>(std::size(ws)), ws,
                0, nullptr);
        };

    fill(/* set 0 -> ping buffer is READ */ 0u, true  /*pingIsRead*/);
    fill(/* set 1 -> pong buffer is READ */ 1u, false /*pingIsRead*/);
}



void VoxelizerPass::PerformEikonalSweeps(VkCommandBuffer cmd, uint32_t curFrame) 
{
    QTDoughApplication* app = QTDoughApplication::instance;
    bool pingRead = true;
    int iterations = 0;

    if (iterations == 0)
        pingRead = false;

    for (; iterations < 0; ++iterations)
    {
        for (int sweep = 0; sweep < 8; ++sweep)
        {
            VkDescriptorSet sets[2] = {
                app->globalDescriptorSets[curFrame],          // set 0
                sweepSets[pingRead ? 0 : 1]                 // set 1 (ping-pong)
            };


            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                voxelizeComputePipelineLayout,
                /*firstSet*/0, /*setCount*/2,
                sets, 0, nullptr);

            PushConsts pc{ float(6 + (sweep % 8)),
                           uint32_t(vertices.size() / 3) };
            vkCmdPushConstants(cmd, voxelizeComputePipelineLayout,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(pc), &pc);

            const uint32_t groups = (VOXEL_RESOLUTIONL1 + 7) / 8;
            vkCmdDispatch(cmd, groups, groups, groups);

            /* visibility barrier */
            VkMemoryBarrier2 mem{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
            mem.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            mem.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            mem.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            mem.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            dep.memoryBarrierCount = 1;
            dep.pMemoryBarriers = &mem;
            vkCmdPipelineBarrier2(cmd, &dep);

            pingRead = !pingRead;
        }

    }
    currentSdfSet = sweepSets[pingRead];
}


void VoxelizerPass::Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
    QTDoughApplication* app = QTDoughApplication::instance;

    UpdateBrushesGPU(commandBuffer);
    float  f = 100.0f;
    uint32_t pattern;
    memcpy(&pattern, &f, sizeof(pattern));   // generate the bit pattern once

    const VkDeviceSize whole = VK_WHOLE_SIZE;         // fill the entire buf

    for (int pp = 0; pp < 2; ++pp) {
        vkCmdFillBuffer(commandBuffer, voxL1PingPong[pp], 0, whole, pattern);
        vkCmdFillBuffer(commandBuffer, voxL2PingPong[pp], 0, whole, pattern);
        vkCmdFillBuffer(commandBuffer, voxL3PingPong[pp], 0, whole, pattern);
    }

    /*  <-- INSERT THIS BARRIER ---------------------------------------------- */
    VkMemoryBarrier2 mem{};
    mem.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    mem.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;            //  writes
    mem.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    mem.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;      //  reads
    mem.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    VkDependencyInfo dep2{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep2.memoryBarrierCount = 1;
    dep2.pMemoryBarriers = &mem;
    vkCmdPipelineBarrier2(commandBuffer, &dep2);
    /* ----------------------------------------------------------------------- */

    //Image is transitioned to WRITE.
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


    //Volume textures to WRITE.
    std::vector<VkImageMemoryBarrier> volumeBarriers(app->textures3D.size());

    for (auto it = app->textures3D.begin(); it != app->textures3D.end(); ++it) {
        const std::string& key = it->first;
        Unigma3DTexture& texture = it->second;
        size_t i = std::distance(app->textures3D.begin(), it); // Get the index of the current texture

        volumeBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        volumeBarriers[i].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        volumeBarriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        volumeBarriers[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        volumeBarriers[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        volumeBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        volumeBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        volumeBarriers[i].image = texture.u_image;
        volumeBarriers[i].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    }

    // Execute barrier
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(volumeBarriers.size()), volumeBarriers.data()
    );

    //Dispatch only for unique volume textures as they might be shared between brushes.
    //Volume textures by index already processed.
    std::unordered_map<uint32_t, uint32_t> textureIndexMap;
    if (dispatchCount < 2)
    {
        DispatchLOD(commandBuffer, currentFrame, 0); //Clear.

        //Write information to volume texture. LOD level acts as which volume texture to write to.

        for(uint32_t i = 0; i < brushes.size(); i++)
		{
            uint32_t index = brushes[i].textureID;

            if(textureIndexMap.find(index) != textureIndexMap.end()) {
				//Already processed this texture.
				continue;
			}
            if (brushes[i].type == 0) { //Mesh type

                std::cout << "Dispatching tile for brush: " << i << " with texture ID: " << index << "Vertex offset: " << brushes[i].vertexOffset << std::endl;
                DispatchBrushCreation(commandBuffer, currentFrame, index);
            }
            textureIndexMap[index] = i; //Mark this texture as processed.
		}
    }


    //Set Volume textures to READ.
    std::vector<VkImageMemoryBarrier> volumeToRead(app->textures3D.size());

    for (auto it = app->textures3D.begin(); it != app->textures3D.end(); ++it) {
        const std::string& key = it->first;
        Unigma3DTexture& texture = it->second;
        size_t i = std::distance(app->textures3D.begin(), it); // Get the index of the current texture

        volumeToRead[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        volumeToRead[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        volumeToRead[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        volumeToRead[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        volumeToRead[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        volumeToRead[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        volumeToRead[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        volumeToRead[i].image = texture.u_image;
        volumeToRead[i].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(volumeToRead.size()), volumeToRead.data()
    );



    if(dispatchCount > 1)
	{
		//Dispatch the tile generation.

        DispatchTile(commandBuffer, currentFrame, 5); //Clear Count.
        //DispatchBrushDeformation(commandBuffer, currentFrame, 1);
        DispatchTile(commandBuffer, currentFrame, 0); //Tile generation.
	}

    if (dispatchCount > 2)
    {
        
        //DispatchLOD(commandBuffer, currentFrame, 0); //Clear.
        //DispatchLOD(commandBuffer, currentFrame, 0); //Clear.
        DispatchLOD(commandBuffer, currentFrame, 3); //Used to cull later stages.
        DispatchLOD(commandBuffer, currentFrame, 2);
        DispatchLOD(commandBuffer, currentFrame, 1);

        //DispatchTile(commandBuffer, currentFrame, 2); Particle to voxel TEST. 

        auto copyToPing = [&](VkBuffer src, VkBuffer dst, VkDeviceSize sz)
            {
                VkBufferCopy region{};
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = sz;
                vkCmdCopyBuffer(commandBuffer, src, dst, 1, &region);
            };

        copyToPing(voxelL1StorageBuffers[currentFrame], voxL1PingPong[0],
            sizeof(Voxel) * VOXEL_COUNTL1);
        copyToPing(voxelL2StorageBuffers[currentFrame], voxL2PingPong[0],
            sizeof(Voxel) * VOXEL_COUNTL2);
        copyToPing(voxelL3StorageBuffers[currentFrame], voxL3PingPong[0],
            sizeof(Voxel) * VOXEL_COUNTL3);

        /* barrier so the sweeps can READ what we just copied */
        VkMemoryBarrier2 mb{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        mb.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        mb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        mb.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        mb.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

        VkDependencyInfo di{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        di.memoryBarrierCount = 1;
        di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(commandBuffer, &di);


        PerformEikonalSweeps(commandBuffer, currentFrame);
        //Finally use Eikonal Equation to propagate the SDF. 6-13
        //DispatchLOD(commandBuffer, currentFrame, 6);
        //DispatchLOD(commandBuffer, currentFrame, 7, true);
        /*
        DispatchLOD(commandBuffer, currentFrame, 8);
        DispatchLOD(commandBuffer, currentFrame, 9, true);
        DispatchLOD(commandBuffer, currentFrame, 10);
        DispatchLOD(commandBuffer, currentFrame, 11, true);
        DispatchLOD(commandBuffer, currentFrame, 12);
        DispatchLOD(commandBuffer, currentFrame, 13, true);
        */
    }

    if (dispatchCount < 2)
    {
        //DispatchLOD(commandBuffer, currentFrame, 5); //Per triangle.
    }

    dispatchCount += 1;


    VkImageMemoryBarrier2 barrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier2.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.image = image;
    barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1 };

    VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier2;

    vkCmdPipelineBarrier2(commandBuffer, &dep);

}

void VoxelizerPass::DispatchBrushCreation(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    //Get the volume texture from the lodlevel.
    Unigma3DTexture& volumeTexture = app->textures3D["brush_" + std::to_string(lodLevel)];

    uint32_t resolutionx = volumeTexture.WIDTH;
    uint32_t resolutiony = volumeTexture.HEIGHT;
    uint32_t resolutionz = volumeTexture.DEPTH;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelizeComputePipeline);

    PushConsts pc{};
    pc.lod = 5;
    pc.triangleCount = lodLevel;

    vkCmdPushConstants(
        commandBuffer,
        voxelizeComputePipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConsts),
        &pc
    );

    VkDescriptorSet sets[] = {
        app->globalDescriptorSets[currentFrame],
        computeDescriptorSets[currentFrame]
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        voxelizeComputePipelineLayout,
        0, 2, sets,
        0, nullptr);

    uint32_t groupCountX = (resolutionx + 7) / 8;
    uint32_t groupCountY = (resolutiony + 7) / 8;
    uint32_t groupCountZ = (resolutionz + 7) / 8;


    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void VoxelizerPass::DispatchBrushDeformation(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t brushID)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    //Get the volume texture from the lodlevel.
    Unigma3DTexture& volumeTexture = app->textures3D["brush_" + std::to_string(brushID)];

    uint32_t resolutionx = volumeTexture.WIDTH;
    uint32_t resolutiony = volumeTexture.HEIGHT;
    uint32_t resolutionz = volumeTexture.DEPTH;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelizeComputePipeline);

    PushConsts pc{};
    pc.lod = 14;
    pc.triangleCount = brushID;

    vkCmdPushConstants(
        commandBuffer,
        voxelizeComputePipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConsts),
        &pc
    );

    VkDescriptorSet sets[] = {
        app->globalDescriptorSets[currentFrame],
        computeDescriptorSets[currentFrame]
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        voxelizeComputePipelineLayout,
        0, 2, sets,
        0, nullptr);

    uint32_t groupCountX = (resolutionx + 7) / 8;
    uint32_t groupCountY = (resolutiony + 7) / 8;
    uint32_t groupCountZ = (resolutionz + 7) / 8;


    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void VoxelizerPass::DispatchTile(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    uint32_t resolutionx = brushes.size();
    uint32_t resolutiony = 1;
    uint32_t resolutionz = 1;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, tileGenerationComputePipeline);

    PushConsts pc{};
    pc.lod = static_cast<float>(lodLevel);
    pc.triangleCount = static_cast<uint32_t>(vertices.size() / 3);

    vkCmdPushConstants(
        commandBuffer,
        tileGenerationComputePipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConsts),
        &pc
    );

    VkDescriptorSet sets[] = {
        app->globalDescriptorSets[currentFrame],
        computeDescriptorSets[currentFrame]
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        tileGenerationComputePipelineLayout,
        0, 2, sets,
        0, nullptr);

    if( lodLevel == 5) {
		//This is the clear dispatch.
        resolutionx = TilesBrushCounts.size();
	}

    if (lodLevel == 2) {
        //This is the clear dispatch.
        resolutionx = PARTICLE_COUNT;
    }


    uint32_t groupCountX = (resolutionx + 7) / 8;
    uint32_t groupCountY = (resolutiony + 7) / 8;
    uint32_t groupCountZ = (resolutionz + 7) / 8;


    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

    VkMemoryBarrier2 mem{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    mem.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    mem.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    mem.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    mem.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

    VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &mem;

    vkCmdPipelineBarrier2(commandBuffer, &dep);
}

void VoxelizerPass::DispatchLOD(VkCommandBuffer commandBuffer, uint32_t currentFrame, uint32_t lodLevel, bool pingFlag)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    uint32_t prevFrame = (currentFrame + app->MAX_FRAMES_IN_FLIGHT - 1) %
        app->MAX_FRAMES_IN_FLIGHT;

    //if (lodLevel >= 6.0f)
        //ping = !ping;

    //BindVoxelBuffers(currentFrame, prevFrame, ping);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelizeComputePipeline);

    PushConsts pc{};
    pc.lod = static_cast<float>(lodLevel);
    pc.triangleCount = static_cast<uint32_t>(vertices.size() / 3);

    vkCmdPushConstants(
        commandBuffer,
        voxelizeComputePipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConsts),
        &pc
    );

    VkDescriptorSet sets[] = {
        app->globalDescriptorSets[currentFrame],
        computeDescriptorSets[currentFrame]
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        voxelizeComputePipelineLayout,
        0, 2, sets,
        0, nullptr);

    // Each LOD uses a different resolution
    uint32_t res = (lodLevel == 1) ? VOXEL_RESOLUTIONL1 :
        (lodLevel == 2) ? VOXEL_RESOLUTIONL2 :
        VOXEL_RESOLUTIONL3;

    uint32_t groupCountX = (res + 7) / 8;
    uint32_t groupCountY = (res + 7) / 8;
    uint32_t groupCountZ = (res + 7) / 8;

    if (lodLevel == 5)
    {
        res = pc.triangleCount;
        groupCountX = (res + 7) / 8;
        groupCountY = (1 + 7) / 8;
        groupCountZ = (1 + 7) / 8;
    }

    if (lodLevel == 0)
    {
        res = VOXEL_RESOLUTIONL1;
        groupCountX = (res + 7) / 8;
        groupCountY = (res + 7) / 8;
        groupCountZ = (res + 7) / 8;
    }

    if (lodLevel >= 6)
    {
        res = VOXEL_RESOLUTIONL1;
        groupCountX = (res + 7) / 8;
        groupCountY = (res + 7) / 8;
        groupCountZ = (res + 7) / 8;
    }


    /*
    // Add debug label for Nsight
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = (lodLevel == 1) ? "LOD1 Dispatch" :
        (lodLevel == 2) ? "LOD2 Dispatch" : "LOD3 Dispatch";
    vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
    */


    
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

    VkMemoryBarrier2 mem{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    mem.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    mem.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    mem.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    mem.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

    VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &mem;

    vkCmdPipelineBarrier2(commandBuffer, &dep);

    //vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}


void VoxelizerPass::BindVoxelBuffers(uint32_t curFrame, uint32_t prevFrame, bool pingFlag)     
{
    QTDoughApplication* app = QTDoughApplication::instance;
    auto buf = [&](std::vector<VkBuffer>& v, uint32_t i) { return v[i]; };

    VkDescriptorBufferInfo rL1{ buf(voxelL1StorageBuffers, pingFlag ? prevFrame : curFrame), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo wL1{ buf(voxelL1StorageBuffers, pingFlag ? curFrame : prevFrame), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo rL2{ buf(voxelL2StorageBuffers, pingFlag ? prevFrame : curFrame), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo wL2{ buf(voxelL2StorageBuffers, pingFlag ? curFrame : prevFrame), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo rL3{ buf(voxelL3StorageBuffers, pingFlag ? prevFrame : curFrame), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo wL3{ buf(voxelL3StorageBuffers, pingFlag ? curFrame : prevFrame), 0, VK_WHOLE_SIZE };

    VkWriteDescriptorSet ws[6]{};
    for (int i = 0; i < 6; ++i) {
        ws[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ws[i].dstSet = computeDescriptorSets[curFrame];
        ws[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ws[i].descriptorCount = 1;
    }
    ws[0].dstBinding = 2; ws[0].pBufferInfo = &rL1;
    ws[1].dstBinding = 3; ws[1].pBufferInfo = &wL1;
    ws[2].dstBinding = 4; ws[2].pBufferInfo = &rL2;
    ws[3].dstBinding = 5; ws[3].pBufferInfo = &wL2;
    ws[4].dstBinding = 6; ws[4].pBufferInfo = &rL3;
    ws[5].dstBinding = 7; ws[5].pBufferInfo = &wL3;

    vkUpdateDescriptorSets(app->_logicalDevice, 6, ws, 0, nullptr);
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