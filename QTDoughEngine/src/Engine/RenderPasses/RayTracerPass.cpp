#include "RayTracerPass.h"
#include "VoxelizerPass.h"
#include <random>

RayTracerPass::~RayTracerPass()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    for (auto& F : rtAS)
    {
        if (F.blas) vkDestroyAccelerationStructureKHR_fn(app->_logicalDevice, F.blas, nullptr);
        if (F.tlas) vkDestroyAccelerationStructureKHR_fn(app->_logicalDevice, F.tlas, nullptr);

        if (F.blasBuffer) vkDestroyBuffer(app->_logicalDevice, F.blasBuffer, nullptr);
        if (F.tlasBuffer) vkDestroyBuffer(app->_logicalDevice, F.tlasBuffer, nullptr);
        if (F.scratchBuffer) vkDestroyBuffer(app->_logicalDevice, F.scratchBuffer, nullptr);
        if (F.instanceBuffer) vkDestroyBuffer(app->_logicalDevice, F.instanceBuffer, nullptr);

        if (F.blasMemory) vkFreeMemory(app->_logicalDevice, F.blasMemory, nullptr);
        if (F.tlasMemory) vkFreeMemory(app->_logicalDevice, F.tlasMemory, nullptr);
        if (F.scratchMemory) vkFreeMemory(app->_logicalDevice, F.scratchMemory, nullptr);
        if (F.instanceMemory) vkFreeMemory(app->_logicalDevice, F.instanceMemory, nullptr);

        F = {};
    }

    PassName = "RayTracerPass";
}

RayTracerPass::RayTracerPass() {
    PassName = "RayTracerPass";
    PassNames.push_back("RayTracerPass");


    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

static inline VkDeviceSize AlignUpVk(VkDeviceSize v, VkDeviceSize a) { return (v + a - 1) & ~(a - 1); }


void RayTracerPass::AddObjects(UnigmaRenderingObject* unigmaRenderingObjects)
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

void RayTracerPass::CreateTriangleSoup()
{
    QTDoughApplication* app = QTDoughApplication::instance;

}

void RayTracerPass::CreateImages() {
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
        passWidth,
        passHeight,
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

    //Creating the rest of the images.
    for (int i = 0; i < images.size(); i++)
    {
        app->CreateImage(
            passWidth,
            passHeight,
            imageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            images[i],
            imagesMemory[i]
        );

        VkImageView imageView = app->CreateImageView(images[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        imagesViews.push_back(imageView);

        UnigmaTexture texture;
        texture.u_image = images[i];
        texture.u_imageView = imageView;
        texture.u_imageMemory = imagesMemory[i];
        texture.TEXTURE_PATH = PassNames[i];

        app->textures.insert({ PassNames[i], texture });
    }
}

void RayTracerPass::CreateUniformBuffers()
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


void RayTracerPass::CreateMaterials() {
    material.Clean();
    material.shader = UnigmaShader("raygen");


    material.textureNames[0] = "RayAlbedoPass";
    material.textureNames[1] = "RayNormalPass";
    material.textureNames[2] = "RayDepthPass";

    images.resize(3);
    imagesMemory.resize(3);
    imagesViews.resize(3);
    PassNames.push_back("RayAlbedoPass");
    PassNames.push_back("RayNormalPass");
    PassNames.push_back("RayDepthPass");

}

void RayTracerPass::UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) {

    QTDoughApplication* app = QTDoughApplication::instance;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.proj = glm::mat4(1.0f);
    ubo.texelSize = glm::vec4(glm::vec2(1.0f / app->swapChainExtent.width, 1.0f / app->swapChainExtent.height), 0, 0);
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
    barrier.buffer = intArrayBuffer;
    barrier.offset = 0;
    barrier.size = bufferSize;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

}


void RayTracerPass::CreateComputePipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    std::cout << "Creating Ray Tracing Pipeline..." << std::endl;
    //Load RT function pointers
    vkCreateRayTracingPipelinesKHR_fn =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkCreateRayTracingPipelinesKHR");
    vkGetRayTracingShaderGroupHandlesKHR_fn =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");
    vkCmdTraceRaysKHR_fn =
        (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkCmdTraceRaysKHR");

    vkGetBufferDeviceAddress_fn =
        (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(app->_logicalDevice, "vkGetBufferDeviceAddress");
    vkGetPhysicalDeviceProperties2_fn =
        (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(app->_vkInstance, "vkGetPhysicalDeviceProperties2");


    vkCreateAccelerationStructureKHR_fn =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkCreateAccelerationStructureKHR");
    vkDestroyAccelerationStructureKHR_fn =
        (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkDestroyAccelerationStructureKHR");
    vkGetAccelerationStructureBuildSizesKHR_fn =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
    vkCmdBuildAccelerationStructuresKHR_fn =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkCmdBuildAccelerationStructuresKHR");
    vkGetAccelerationStructureDeviceAddressKHR_fn =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR");


    if (!vkCmdTraceRaysKHR_fn || !vkCreateRayTracingPipelinesKHR_fn || !vkCreateAccelerationStructureKHR_fn) {
        throw std::runtime_error("RTX function pointers null. Extensions/features not enabled.");
    }

    //Query RT pipeline properties
    VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(app->_physicalDevice, &props2);

    //Create Remaining Pipeline:
    VoxelizerPass* voxelizer = VoxelizerPass::instance;

    std::cout << "Loading ray tracing shader files..." << std::endl;

    auto loadRayTracingShaders = [](const char* path, QTDoughApplication* app) -> VkShaderModule {
        std::vector<char> filePath = readFile(path);
        return app->CreateShaderModule(filePath);

    };

    VkShaderModule rgen = loadRayTracingShaders("src/shaders/raygen.spv", app);
    VkShaderModule rmiss = loadRayTracingShaders("src/shaders/miss.spv", app);
    VkShaderModule rahit = loadRayTracingShaders("src/shaders/anyhit.spv", app);
    VkShaderModule rchit = loadRayTracingShaders("src/shaders/closesthit.spv", app);


    std::array<VkPipelineShaderStageCreateInfo, 4> stages{};
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,        rgen,  "main" };
    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR,          rmiss, "main" };
    stages[2] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR,       rahit, "main" };
    stages[3] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,   rchit, "main" };

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};

    // raygen group
    groups[0] = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;

    // miss group
    groups[1] = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader = 1;

    // triangles hit group: anyhit + closesthit
    groups[2] = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].anyHitShader = 2;
    groups[2].closestHitShader = 3;
    groups[2].generalShader = VK_SHADER_UNUSED_KHR;
    groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;



    std::array<VkDescriptorSetLayout, 2> setLayouts = {
        app->globalDescriptorSetLayout, // set = 0
        rtDescriptorSetLayout           // set = 1
    };

    VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pl.setLayoutCount = (uint32_t)setLayouts.size();
    pl.pSetLayouts = setLayouts.data();
    VK_CHECK(vkCreatePipelineLayout(app->_logicalDevice, &pl, nullptr, &rtPipelineLayout));


    VkRayTracingPipelineCreateInfoKHR pci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    pci.stageCount = (uint32_t)stages.size();
    pci.pStages = stages.data();
    pci.groupCount = (uint32_t)groups.size();
    pci.pGroups = groups.data();
    pci.maxPipelineRayRecursionDepth = 1;
    pci.layout = rtPipelineLayout;

    VK_CHECK(vkCreateRayTracingPipelinesKHR_fn(app->_logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pci, nullptr, &rtPipeline));

    vkDestroyShaderModule(app->_logicalDevice, rgen, nullptr);
    vkDestroyShaderModule(app->_logicalDevice, rmiss, nullptr);
    vkDestroyShaderModule(app->_logicalDevice, rchit, nullptr);
    vkDestroyShaderModule(app->_logicalDevice, rahit, nullptr);

    std::cout << "Ray Tracing Pipeline created successfully." << std::endl;

}

void RayTracerPass::CreateComputeDescriptorSetLayout()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    // Create intArrayBuffer exactly like VoxelizerPass
    VkDeviceSize bufferSize = sizeof(uint32_t) * MAX_NUM_TEXTURES;
    app->CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        intArrayBuffer,
        intArrayBufferMemory
    );

    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorCount = 1;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding ids{};
    ids.binding = 1;
    ids.descriptorCount = 1;
    ids.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ids.stageFlags = ubo.stageFlags;

    VkDescriptorSetLayoutBinding tlas{};
    tlas.binding = 2;
    tlas.descriptorCount = 1;
    tlas.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    tlas.stageFlags = ubo.stageFlags;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { ubo, ids, tlas };

    VkDescriptorSetLayoutCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = (uint32_t)bindings.size();
    info.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(app->_logicalDevice, &info, nullptr, &rtDescriptorSetLayout));
}


void RayTracerPass::CreateDescriptorPool()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = app->MAX_FRAMES_IN_FLIGHT;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = app->MAX_FRAMES_IN_FLIGHT;

    poolSizes[2].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[2].descriptorCount = app->MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = app->MAX_FRAMES_IN_FLIGHT;

    VK_CHECK(vkCreateDescriptorPool(app->_logicalDevice, &poolInfo, nullptr, &descriptorPool));
}



void RayTracerPass::CreateComputeDescriptorSets()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    rtDescriptorSets.resize(app->MAX_FRAMES_IN_FLIGHT);
    rtAS.resize(app->MAX_FRAMES_IN_FLIGHT);

    std::vector<VkDescriptorSetLayout> layouts(app->MAX_FRAMES_IN_FLIGHT, rtDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = (uint32_t)rtDescriptorSets.size();
    allocInfo.pSetLayouts = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(app->_logicalDevice, &allocInfo, rtDescriptorSets.data()));

    for (uint32_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = _uniformBuffers[i];
        uboInfo.offset = 0;
        uboInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo idInfo{};
        idInfo.buffer = intArrayBuffer;
        idInfo.offset = 0;
        idInfo.range = VK_WHOLE_SIZE;

        std::array<VkWriteDescriptorSet, 2> ws{};

        ws[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ws[0].dstSet = rtDescriptorSets[i];
        ws[0].dstBinding = 0;
        ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ws[0].descriptorCount = 1;
        ws[0].pBufferInfo = &uboInfo;

        ws[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ws[1].dstSet = rtDescriptorSets[i];
        ws[1].dstBinding = 1;
        ws[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ws[1].descriptorCount = 1;
        ws[1].pBufferInfo = &idInfo;

        vkUpdateDescriptorSets(app->_logicalDevice, (uint32_t)ws.size(), ws.data(), 0, nullptr);
    }
}


void RayTracerPass::CreateShaderStorageBuffers()
{

}

void RayTracerPass::CreateSBT()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    uint32_t handleSize = rtProps.shaderGroupHandleSize;
    uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
    uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;

    auto AlignUp = [](uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); };
    uint32_t handleSizeAligned = AlignUp(handleSize, handleAlign);

    // groups: [0]=rgen, [1]=miss, [2]=hit
    uint32_t raygenSize = AlignUp(handleSizeAligned, baseAlign);
    uint32_t missSize = AlignUp(handleSizeAligned, baseAlign);
    uint32_t hitSize = AlignUp(handleSizeAligned, baseAlign);
    uint32_t sbtSize = raygenSize + missSize + hitSize;

    std::vector<uint8_t> handles(3 * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR_fn(
        app->_logicalDevice, rtPipeline, 0, 3,
        (uint32_t)handles.size(), handles.data()));

    app->CreateBuffer(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // optional
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtBuffer, sbtMemory);

    uint8_t* mapped = nullptr;
    vkMapMemory(app->_logicalDevice, sbtMemory, 0, sbtSize, 0, (void**)&mapped);
    memset(mapped, 0, sbtSize);

    memcpy(mapped + 0, handles.data() + 0 * handleSize, handleSize); // rgen
    memcpy(mapped + raygenSize, handles.data() + 1 * handleSize, handleSize); // miss
    memcpy(mapped + raygenSize + missSize, handles.data() + 2 * handleSize, handleSize); // hit

    vkUnmapMemory(app->_logicalDevice, sbtMemory);

    VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = sbtBuffer;
    VkDeviceAddress sbtAddr = vkGetBufferDeviceAddress_fn(app->_logicalDevice, &addrInfo);

    raygenRegion.deviceAddress = sbtAddr + 0;
    raygenRegion.stride = raygenSize;
    raygenRegion.size = raygenSize;

    missRegion.deviceAddress = sbtAddr + raygenSize;
    missRegion.stride = missSize;
    missRegion.size = missSize;

    hitRegion.deviceAddress = sbtAddr + raygenSize + missSize;
    hitRegion.stride = hitSize;
    hitRegion.size = hitSize;

    callableRegion = {};
}

VkDeviceAddress RayTracerPass::GetBufferAddress(VkBuffer buffer)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = buffer;
    return vkGetBufferDeviceAddress_fn(app->_logicalDevice, &info);
}

VkDeviceAddress RayTracerPass::GetASAddress(VkAccelerationStructureKHR as)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR
    };
    info.accelerationStructure = as;
    return vkGetAccelerationStructureDeviceAddressKHR_fn(app->_logicalDevice, &info);
}

void RayTracerPass::BuildBLAS_FromTriangleSoup(
    VkCommandBuffer cmd,
    uint32_t frame,
    VkBuffer vertexBuffer, VkDeviceSize vertexOffset,
    uint32_t vertexCount, VkDeviceSize vertexStride)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    auto& F = rtAS[frame];

    if (vertexCount < 3) return;

    std::cout << "Vertex Count for BLAS: " << vertexCount << std::endl;

    VkDeviceSize posOffset = offsetof(Vertex, pos);

    VkDeviceAddress vaddr = GetBufferAddress(vertexBuffer) + vertexOffset + posOffset;

    VkAccelerationStructureGeometryTrianglesDataKHR tri{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR
    };
    tri.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT; 
    tri.vertexData.deviceAddress = vaddr;
    tri.vertexStride = vertexStride;
    tri.maxVertex = vertexCount - 1;
    tri.indexType = VK_INDEX_TYPE_NONE_KHR;

    VkAccelerationStructureGeometryKHR geom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
    };
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles = tri;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = vertexCount / 3;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
    };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    uint32_t primCount = range.primitiveCount;

    vkGetAccelerationStructureBuildSizesKHR_fn(
        app->_logicalDevice,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primCount,
        &sizes);

    if (F.blas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR_fn(app->_logicalDevice, F.blas, nullptr);
        vkDestroyBuffer(app->_logicalDevice, F.blasBuffer, nullptr);
        vkFreeMemory(app->_logicalDevice, F.blasMemory, nullptr);
        F.blas = VK_NULL_HANDLE;
        F.blasBuffer = VK_NULL_HANDLE;
        F.blasMemory = VK_NULL_HANDLE;
        F.blasAddr = 0;
    }

    app->CreateBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        F.blasBuffer, F.blasMemory);

    VkAccelerationStructureCreateInfoKHR asCreate{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR
    };
    asCreate.buffer = F.blasBuffer;
    asCreate.size = sizes.accelerationStructureSize;
    asCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VK_CHECK(vkCreateAccelerationStructureKHR_fn(app->_logicalDevice, &asCreate, nullptr, &F.blas));


    if (F.scratchBuffer == VK_NULL_HANDLE || sizes.buildScratchSize > F.scratchSize) {
        if (F.scratchBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(app->_logicalDevice, F.scratchBuffer, nullptr);
            vkFreeMemory(app->_logicalDevice, F.scratchMemory, nullptr);
        }
        F.scratchSize = sizes.buildScratchSize;
        app->CreateBuffer(
            F.scratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            F.scratchBuffer, F.scratchMemory);
    }

    buildInfo.dstAccelerationStructure = F.blas;
    buildInfo.scratchData.deviceAddress = GetBufferAddress(F.scratchBuffer);

    vkCmdBuildAccelerationStructuresKHR_fn(cmd, 1, &buildInfo, &pRange);


    VkMemoryBarrier mb{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    mb.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &mb, 0, nullptr, 0, nullptr);

    F.blasAddr = GetASAddress(F.blas);
}

void RayTracerPass::BuildTLAS_SingleInstance(
    VkCommandBuffer cmd,
    uint32_t frame,
    const VkTransformMatrixKHR& xform)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    auto& F = rtAS[frame];

    // Instance points at THIS FRAME'S BLAS
    VkAccelerationStructureInstanceKHR inst{};
    inst.transform = xform;
    inst.instanceCustomIndex = 0;
    inst.mask = 0xFF;
    inst.instanceShaderBindingTableRecordOffset = 0;
    inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    inst.accelerationStructureReference = F.blasAddr;


    if (F.instanceBuffer == VK_NULL_HANDLE) {
        app->CreateBuffer(
            sizeof(VkAccelerationStructureInstanceKHR),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            F.instanceBuffer, F.instanceMemory);
    }

    void* mapped = nullptr;
    vkMapMemory(app->_logicalDevice, F.instanceMemory, 0, sizeof(inst), 0, &mapped);
    memcpy(mapped, &inst, sizeof(inst));
    vkUnmapMemory(app->_logicalDevice, F.instanceMemory);

    VkDeviceAddress instAddr = GetBufferAddress(F.instanceBuffer);

    VkAccelerationStructureGeometryInstancesDataKHR instances{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR
    };
    instances.arrayOfPointers = VK_FALSE;
    instances.data.deviceAddress = instAddr;

    VkAccelerationStructureGeometryKHR geom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
    };
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances = instances;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = 1;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
    };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    uint32_t primCount = 1;

    vkGetAccelerationStructureBuildSizesKHR_fn(
        app->_logicalDevice,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primCount,
        &sizes);


    if (F.tlas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR_fn(app->_logicalDevice, F.tlas, nullptr);
        vkDestroyBuffer(app->_logicalDevice, F.tlasBuffer, nullptr);
        vkFreeMemory(app->_logicalDevice, F.tlasMemory, nullptr);
        F.tlas = VK_NULL_HANDLE;
        F.tlasBuffer = VK_NULL_HANDLE;
        F.tlasMemory = VK_NULL_HANDLE;
        F.tlasAddr = 0;
    }

    app->CreateBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        F.tlasBuffer, F.tlasMemory);

    VkAccelerationStructureCreateInfoKHR asCreate{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR
    };
    asCreate.buffer = F.tlasBuffer;
    asCreate.size = sizes.accelerationStructureSize;
    asCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VK_CHECK(vkCreateAccelerationStructureKHR_fn(app->_logicalDevice, &asCreate, nullptr, &F.tlas));


    if (F.scratchBuffer == VK_NULL_HANDLE || sizes.buildScratchSize > F.scratchSize) {
        if (F.scratchBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(app->_logicalDevice, F.scratchBuffer, nullptr);
            vkFreeMemory(app->_logicalDevice, F.scratchMemory, nullptr);
        }
        F.scratchSize = sizes.buildScratchSize;
        app->CreateBuffer(
            F.scratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            F.scratchBuffer, F.scratchMemory);
    }

    buildInfo.dstAccelerationStructure = F.tlas;
    buildInfo.scratchData.deviceAddress = GetBufferAddress(F.scratchBuffer);

    vkCmdBuildAccelerationStructuresKHR_fn(cmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier mb{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    mb.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &mb, 0, nullptr, 0, nullptr);

    F.tlasAddr = GetASAddress(F.tlas);
}


void RayTracerPass::Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VoxelizerPass* voxelizer = VoxelizerPass::instance;

    CreateSBT();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 0, nullptr, 0, nullptr, 1, &barrier);


    BuildBLAS_FromTriangleSoup(commandBuffer, currentFrame, voxelizer->meshingVertexBuffer, 0, voxelizer->readBackVertexCount, sizeof(Vertex));
    BuildTLAS_SingleInstance(commandBuffer, currentFrame);

    VkAccelerationStructureKHR tlasForFrame = rtAS[currentFrame].tlas;

    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
    };
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &tlasForFrame;

    VkWriteDescriptorSet tlasWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    tlasWrite.dstSet = rtDescriptorSets[currentFrame];
    tlasWrite.dstBinding = 2;
    tlasWrite.descriptorCount = 1;
    tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    tlasWrite.pNext = &asInfo;

    vkUpdateDescriptorSets(app->_logicalDevice, 1, &tlasWrite, 0, nullptr);


    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);


    VkDescriptorSet sets[2] = {
        app->globalDescriptorSets[currentFrame], // set 0
        rtDescriptorSets[currentFrame]           // set 1
    };

    vkCmdBindDescriptorSets(commandBuffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        rtPipelineLayout,
        0, 2, sets,
        0, nullptr);

    auto transitionImage = [&](VkImage img,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlags srcAccess,
        VkAccessFlags dstAccess,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage)
        {
            VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            b.oldLayout = oldLayout;
            b.newLayout = newLayout;
            b.srcAccessMask = srcAccess;
            b.dstAccessMask = dstAccess;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img;
            b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            vkCmdPipelineBarrier(commandBuffer,
                srcStage, dstStage,
                0, 0, nullptr, 0, nullptr, 1, &b);
        };

    // These are the images raygen is ACTUALLY writing via bindless:
    VkImage albedoImg = app->textures["RayAlbedoPass"].u_image;
    VkImage normalImg = app->textures["RayNormalPass"].u_image;

    // Make them writable for ray tracing:
    transitionImage(albedoImg,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);


    transitionImage(normalImg,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

    vkCmdTraceRaysKHR_fn(commandBuffer,
        &raygenRegion,
        &missRegion,
        &hitRegion,
        &callableRegion,
        (uint32_t)passWidth, (uint32_t)passHeight, 1);

    transitionImage(albedoImg,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    transitionImage(normalImg,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


    VkImageMemoryBarrier barrierBack{};
    barrierBack.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierBack.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierBack.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrierBack.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierBack.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierBack.image = image;
    barrierBack.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrierBack);
}

void RayTracerPass::DebugCompute(uint32_t currentFrame)
{

}