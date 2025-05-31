#include "ComputePass.h"
#include <random>

ComputePass::~ComputePass() {
}

ComputePass::ComputePass() {}

void ComputePass::AddObjects(UnigmaRenderingObject* unigmaRenderingObjects)
{

    QTDoughApplication* app = QTDoughApplication::instance;
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
        {
            renderingObjects.push_back(&unigmaRenderingObjects[i]);
        }
    }
}


void ComputePass::CreateTriangleSoup()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    // Objects should already be added to renderingObjects stack by this point. Now process them.

    // First add all the vertices and indices to the vectors.
    for (int i = 0; i < renderingObjects.size(); i++)
    {
        glm::mat4 model = renderingObjects[i]->_transform.GetModelMatrix();
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model))); // for correct normal transform

        for (auto& vertex : renderingObjects[i]->_renderer.vertices)
        {
            ComputeVertex computeVertex;
            computeVertex.position = model * glm::vec4(vertex.pos, 1.0f);
            computeVertex.texCoord = glm::vec4(vertex.texCoord, 0.0f, 0.0f);
            computeVertex.normal = glm::vec4(normalMatrix * vertex.normal, 0.0f); // use normal matrix


            vertices.push_back(computeVertex);
        }



        // Adjust index offset based on current vertex base
        uint32_t vertexOffset = static_cast<uint32_t>(vertices.size() - renderingObjects[i]->_renderer.vertices.size());
        for (auto idx : renderingObjects[i]->_renderer.indices)
        {
            indices.push_back(idx + vertexOffset);
        }
    }


    VkDeviceSize vertexSize = sizeof(ComputeVertex) * vertices.size();
    VkDeviceSize indexSize = sizeof(glm::uvec3) * (indices.size() / 3); // each triangle = 1 glm::uvec3

    std::cout << "Vertex count: " << vertices.size() << std::endl;
    std::cout << "Vertex size: " << vertexSize << std::endl;


    // Create staging buffers
    VkBuffer stagingVertexBuffer, stagingIndexBuffer;
    VkDeviceMemory stagingVertexMemory, stagingIndexMemory;

    app->CreateBuffer(vertexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingVertexBuffer, stagingVertexMemory);

    app->CreateBuffer(indexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingIndexBuffer, stagingIndexMemory);

    // Copy CPU data into staging buffers
    void* data;
    vkMapMemory(app->_logicalDevice, stagingVertexMemory, 0, vertexSize, 0, &data);
    std::memcpy(data, vertices.data(), vertexSize);
    vkUnmapMemory(app->_logicalDevice, stagingVertexMemory);

    triangleIndices.resize(indices.size() / 3);
    for (size_t i = 0; i < triangleIndices.size(); ++i)
    {
        triangleIndices[i] = glm::uvec3(indices[i * 3], indices[i * 3 + 1], indices[i * 3 + 2]);
    }

    vkMapMemory(app->_logicalDevice, stagingIndexMemory, 0, indexSize, 0, &data);
    std::memcpy(data, triangleIndices.data(), indexSize);
    vkUnmapMemory(app->_logicalDevice, stagingIndexMemory);

    // Create GPU buffers
    app->CreateBuffer(vertexSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer, vertexBufferMemory);

    app->CreateBuffer(indexSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexBufferMemory);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(app->_logicalDevice, vertexBuffer, &req);
    std::cout << "vertexBuffer memReq.size = " << req.size << "\n";

    // Copy from staging to device-local buffers
    app->CopyBuffer(stagingVertexBuffer, vertexBuffer, vertexSize);
    app->CopyBuffer(stagingIndexBuffer, indexBuffer, indexSize);

    // Cleanup staging
    vkDestroyBuffer(app->_logicalDevice, stagingVertexBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingVertexMemory, nullptr);
    vkDestroyBuffer(app->_logicalDevice, stagingIndexBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingIndexMemory, nullptr);
}


void ComputePass::CreateUniformBuffers()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    _uniformBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMapped.resize(app->MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        app->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);

        vkMapMemory(app->_logicalDevice, _uniformBuffersMemory[i], 0, bufferSize, 0, &_uniformBuffersMapped[i]);
    }

}

void ComputePass::UpdateUniformBuffer(uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) {

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

    /*
    vertices.clear();
    //Update all vertices world positions.
    for (int i = 0; i < renderingObjects.size(); i++)
    {

        glm::mat4 model = renderingObjects[i]->_transform.GetModelMatrix();
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model))); // for correct normal transform

        for (auto& vertex : renderingObjects[i]->_renderer.vertices)
        {
            ComputeVertex computeVertex;
            computeVertex.position = model * glm::vec4(vertex.pos, 1.0f);
            computeVertex.texCoord = glm::vec4(vertex.texCoord, 0.0f, 0.0f);
            computeVertex.normal = glm::vec4(normalMatrix * vertex.normal, 0.0f); // use normal matrix

            vertices.push_back(computeVertex);
        }
    }
    */

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


void ComputePass::UpdateUniformBufferObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame, VkImageView* targetImage, UnigmaCameraStruct* CameraMain)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    for (int i = 0; i < renderingObjects.size(); i++)
    {
        renderingObjects[i]->UpdateUniformBuffer(*app, currentFrame, *renderingObjects[i], *CameraMain, _uniformBuffersMapped[currentFrame]);
    }
}

void ComputePass::CreateImages() {
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
    barrier.subresourceRange.levelCount = 1;
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

void ComputePass::CreateComputePipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    auto computeShaderCode = readFile("src/shaders/particletestcompute.spv");
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

    readbackBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    readbackBufferMemories.resize(app->MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; ++i) {
        app->CreateBuffer(
            sizeof(Particle) * PARTICLE_COUNT,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            readbackBuffers[i],
            readbackBufferMemories[i]
        );
    }
}

void ComputePass::CreateComputeDescriptorSetLayout()
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
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional


    //For texture IDs the second binding.
    VkDescriptorSetLayoutBinding intArrayLayoutBinding{};
    intArrayLayoutBinding.binding = 1;
    intArrayLayoutBinding.descriptorCount = 1;
    intArrayLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    intArrayLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
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
    std::array<VkDescriptorSetLayoutBinding, 6> bindings = { uboLayoutBinding, intArrayLayoutBinding, generalBinding, generalBinding2, vertexBinding, indexBinding  };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(app->_logicalDevice, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }

}


void ComputePass::CreateDescriptorPool()
{
    std::cout << "Creating descriptor pools" << std::endl;
    QTDoughApplication* app = QTDoughApplication::instance;
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(app->MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(app->MAX_FRAMES_IN_FLIGHT) *3 + 2; //Last two are used in all frames.

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(app->MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(app->_logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::cout << "Created descriptor pools" << std::endl;
}



void ComputePass::CreateComputeDescriptorSets()
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

        VkDescriptorBufferInfo storageBufferInfoLastFrame{};
        storageBufferInfoLastFrame.buffer = shaderStorageBuffers[(i - 1) % app->MAX_FRAMES_IN_FLIGHT];
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
        storageBufferInfoCurrentFrame.buffer = shaderStorageBuffers[i];
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;


        std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
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
        descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = computeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;


        VkDescriptorBufferInfo iInfo{};
        VkDescriptorBufferInfo vInfo{};
        vInfo.buffer = vertexBuffer;
        vInfo.offset = 0;
        vInfo.range = VK_WHOLE_SIZE;

        iInfo.buffer = indexBuffer;
        iInfo.offset = 0;
        iInfo.range = VK_WHOLE_SIZE;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = computeDescriptorSets[i];
        descriptorWrites[3].dstBinding = 4;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].pBufferInfo = &vInfo;

        descriptorWrites[4] = descriptorWrites[3];
        descriptorWrites[4].dstBinding = 5;
        descriptorWrites[4].pBufferInfo = &iInfo;

        vkUpdateDescriptorSets(app->_logicalDevice, 5, descriptorWrites.data(), 0, nullptr);
    }




    std::cout << "Created descriptor pools" << std::endl;
}

void ComputePass::CreateShaderStorageBuffers()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    // Initialize particles
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles) {
        float r = 0.25f * sqrt(rndDist(rndEngine));
        float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
        float x = r * cos(theta) * app->swapChainExtent.height / app->swapChainExtent.width;
        float y = r * sin(theta);
        particle.position = glm::vec2(x, y);
        particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
        particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }

    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

    // Create a staging buffer used to upload data to the gpu
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    app->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(app->_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, particles.data(), particles.size() * sizeof(Particle));
    vkUnmapMemory(app->_logicalDevice, stagingBufferMemory);

    shaderStorageBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    shaderStorageBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);

    // Copy initial particle data to all storage buffers
    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        app->CreateBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shaderStorageBuffers[i], shaderStorageBuffersMemory[i]);
        app->CopyBuffer(stagingBuffer, shaderStorageBuffers[i], bufferSize);
    }

    vkDestroyBuffer(app->_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingBufferMemory, nullptr);

}

void ComputePass::CreateComputeCommandBuffers()
{
    QTDoughApplication* app = QTDoughApplication::instance;
    computeCommandBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)computeCommandBuffers.size();

    if (vkAllocateCommandBuffers(app->_logicalDevice, &allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute command buffers!");
    }

    std::cout << "Created compute command buffers" << std::endl;
}

void ComputePass::Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
	QTDoughApplication* app = QTDoughApplication::instance;

    if (vkGetFenceStatus(app->_logicalDevice, app->computeFences[currentFrame]) == VK_NOT_READY)
        return; // Wait until readback from this buffer is done


	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

	// Bind the descriptor set
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		computePipelineLayout,
		0, // First set
		1, // Number of sets
		&computeDescriptorSets[currentFrame],
		0, nullptr // No dynamic offsets
	);

	// Dispatch the compute shader
	vkCmdDispatch(commandBuffer, PARTICLE_COUNT / 256, 1, 1);
}

void ComputePass::DebugCompute(uint32_t currentFrame)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

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
    copyRegion.size = sizeof(Particle) * PARTICLE_COUNT;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, readbackBuffers[currentFrame], 1, &copyRegion);

    app->EndSingleTimeCommandsAsync(currentFrame, commandBuffer, [=]() {
        Particle* mappedParticles = nullptr;

        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = readbackBufferMemories[currentFrame];
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;

        vkInvalidateMappedMemoryRanges(app->_logicalDevice, 1, &range);
        vkMapMemory(app->_logicalDevice, readbackBufferMemories[currentFrame], 0, bufferSize, 0, reinterpret_cast<void**>(&mappedParticles));

        /*
        for (size_t i = 0; i < std::min((size_t)10, (size_t)PARTICLE_COUNT); ++i) {
            std::cout << "Particle " << i
                << " Pos: (" << mappedParticles[i].position.x << ", " << mappedParticles[i].position.y << ")"
                << " Vel: (" << mappedParticles[i].velocity.x << ", " << mappedParticles[i].velocity.y << ")"
                << " Color: (" << mappedParticles[i].color.r << ", " << mappedParticles[i].color.g << ", "
                << mappedParticles[i].color.b << ", " << mappedParticles[i].color.a << ")"
                << std::endl;
        }
        */
        vkUnmapMemory(app->_logicalDevice, readbackBufferMemories[currentFrame]);
        });
}


void ComputePass::CreateMaterials() {
    material.Clean();
}