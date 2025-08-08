#include "SDFPass.h"
#include "VoxelizerPass.h"
#include <random>

SDFPass::~SDFPass() {
    PassName = "SDFPass";
}

SDFPass::SDFPass() {
    PassName = "SDFPass";
    PassNames.push_back("SDFPass");


    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

void SDFPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("raymarchsdf");

    material.textureNames[0] = "SDFAlbedoPass";
    material.textureNames[1] = "SDFNormalPass";
    material.textureNames[2] = "SDFPositionPass";
    material.textureNames[3] = "FullSDFField";

    images.resize(3);
    imagesMemory.resize(3);
    imagesViews.resize(3);
    PassNames.push_back("SDFAlbedoPass");
    PassNames.push_back("SDFNormalPass");
    PassNames.push_back("SDFPositionPass");
    PassNames.push_back("FullSDFField");

}

void SDFPass::CreateComputePipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VoxelizerPass* voxelizer = VoxelizerPass::instance;

    auto computeShaderCode = readFile("src/shaders/raymarchsdf.spv");
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
}

void SDFPass::CreateComputeDescriptorSets()
{

}

void SDFPass::UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) {

    QTDoughApplication* app = QTDoughApplication::instance;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.proj = glm::mat4(1.0f);
    ubo.texelSize = glm::vec4(glm::vec2(1.0f / passWidth, 1.0f / passHeight), 0, 0);
    ubo.isOrtho = CameraMain.isOrthogonal;

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

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = intArrayBuffer;
    barrier.offset = 0;
    barrier.size = bufferSize;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );

}


void SDFPass::CreateComputeDescriptorSetLayout()
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

    //Fluid Particles.
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

    //Control Particles.
    VkDescriptorSetLayoutBinding controlParticleBinding1{};
    controlParticleBinding1.binding = 14;
    controlParticleBinding1.descriptorCount = 1;
    controlParticleBinding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    controlParticleBinding1.pImmutableSamplers = nullptr;
    controlParticleBinding1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding controlParticleBinding2{};
    controlParticleBinding2.binding = 15;
    controlParticleBinding2.descriptorCount = 1;
    controlParticleBinding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    controlParticleBinding2.pImmutableSamplers = nullptr;
    controlParticleBinding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Global ID Counter
    VkDescriptorSetLayoutBinding globalIDCounterBinding{};
    globalIDCounterBinding.binding = 16;
    globalIDCounterBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    globalIDCounterBinding.descriptorCount = 1;
    globalIDCounterBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Bind the buffers we specified.
    std::array<VkDescriptorSetLayoutBinding, 17> bindings = { uboLayoutBinding, intArrayLayoutBinding, voxelL1Binding, voxelL1Binding2, voxelL2Binding, voxelL2Binding2, voxelL3Binding, voxelL3Binding2, vertexBinding, brushBinding, brushIndicesBinding, tileBrushCountBinding, particleBinding1, particleBinding2, controlParticleBinding1, controlParticleBinding2, globalIDCounterBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(app->_logicalDevice, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }

}

void SDFPass::CreateShaderStorageBuffers()
{

}


void SDFPass::Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
    QTDoughApplication* app = QTDoughApplication::instance;
    VoxelizerPass* voxelizer = VoxelizerPass::instance;

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
        voxelizer->currentSdfSet
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        voxelizer->voxelizeComputePipelineLayout,
        0, 2, sets,
        0, nullptr);

    uint32_t groupCountX = (passWidth + 7) / 8;
    uint32_t groupCountY = (passHeight + 7) / 8;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

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

void SDFPass::DebugCompute(uint32_t currentFrame)
{

}