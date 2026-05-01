#include "QuantaSpherePass.h"
#include "VoxelizerPass.h"
#include "../Physics/MaterialSimulationPass.h"

QuantaSpherePass::~QuantaSpherePass() {}

QuantaSpherePass::QuantaSpherePass()
{
    QTDoughApplication* app = QTDoughApplication::instance;
    PassName = "QuantaSpherePass";
    PassNames.push_back("QuantaSpherePass");
    passWidth  = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

void QuantaSpherePass::CreateMaterials()
{
    material.Clean();
    material.shader = UnigmaShader("quantasphere");
}

void QuantaSpherePass::CreateImages()
{
    QTDoughApplication* app = QTDoughApplication::instance;
    images.resize(1);
    imagesViews.resize(1);
    imagesMemory.resize(1);

    VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;

    app->CreateImage(
        app->swapChainExtent.width, app->swapChainExtent.height,
        fmt, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        images[0], imagesMemory[0]);

    VkCommandBuffer cmd = app->BeginSingleTimeCommands();
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = images[0];
    b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    b.srcAccessMask = 0;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    app->EndSingleTimeCommands(cmd);

    imagesViews[0] = app->CreateImageView(images[0], fmt, VK_IMAGE_ASPECT_COLOR_BIT);

    UnigmaTexture tex;
    tex.u_image = images[0];
    tex.u_imageView = imagesViews[0];
    tex.u_imageMemory = imagesMemory[0];
    tex.TEXTURE_PATH = PassNames[0];
    app->textures.insert({ PassNames[0], tex });
}

void QuantaSpherePass::CreateUniformBuffers()
{
    QTDoughApplication* app = QTDoughApplication::instance;
    VkDeviceSize size = sizeof(UniformBufferObject);
    _uniformBuffers.resize(app->MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMemory.resize(app->MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMapped.resize(app->MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        app->CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _uniformBuffers[i], _uniformBuffersMemory[i]);
        vkMapMemory(app->_logicalDevice, _uniformBuffersMemory[i], 0, size, 0, &_uniformBuffersMapped[i]);
    }
}

void QuantaSpherePass::CreateDescriptorSetLayout()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    app->CreateBuffer(sizeof(uint32_t) * MAX_NUM_TEXTURES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        intArrayBuffer, intArrayBufferMemory);

    auto mkBinding = [](uint32_t b, VkDescriptorType t) {
        VkDescriptorSetLayoutBinding x{};
        x.binding = b;
        x.descriptorType = t;
        x.descriptorCount = 1;
        x.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        return x;
    };

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
        mkBinding(0,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
        mkBinding(1,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
        mkBinding(7,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
        mkBinding(12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
    };

    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = (uint32_t)bindings.size();
    li.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(app->_logicalDevice, &li, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("QuantaSpherePass: descriptor set layout failed");
}

void QuantaSpherePass::CreateDescriptorPool()
{
    QTDoughApplication* app = QTDoughApplication::instance;
    std::array<VkDescriptorPoolSize, 2> ps{};
    ps[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps[0].descriptorCount = (uint32_t)app->MAX_FRAMES_IN_FLIGHT;
    ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ps[1].descriptorCount = (uint32_t)app->MAX_FRAMES_IN_FLIGHT * 3;

    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.poolSizeCount = (uint32_t)ps.size();
    pi.pPoolSizes = ps.data();
    pi.maxSets = (uint32_t)app->MAX_FRAMES_IN_FLIGHT;
    if (vkCreateDescriptorPool(app->_logicalDevice, &pi, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("QuantaSpherePass: descriptor pool failed");
}

void QuantaSpherePass::CreateDescriptorSets()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    std::vector<VkDescriptorSetLayout> layouts(app->MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = (uint32_t)app->MAX_FRAMES_IN_FLIGHT;
    ai.pSetLayouts = layouts.data();
    descriptorSets.resize(app->MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(app->_logicalDevice, &ai, descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("QuantaSpherePass: alloc descriptor sets failed");

    for (size_t i = 0; i < app->MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo uboInfo    { _uniformBuffers[i], 0, sizeof(UniformBufferObject) };
        VkDescriptorBufferInfo intArrInfo { intArrayBuffer,     0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo brushInfo  { VoxelizerPass::instance->brushesStorageBuffers, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo quantaInfo { MaterialSimulation::instance->GetQuantaBuffer(2), 0, VK_WHOLE_SIZE };

        std::array<VkWriteDescriptorSet, 4> w{};
        for (auto& x : w) x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet = descriptorSets[i]; w[0].dstBinding = 0;  w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount = 1; w[0].pBufferInfo = &uboInfo;
        w[1].dstSet = descriptorSets[i]; w[1].dstBinding = 1;  w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[1].descriptorCount = 1; w[1].pBufferInfo = &intArrInfo;
        w[2].dstSet = descriptorSets[i]; w[2].dstBinding = 7;  w[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[2].descriptorCount = 1; w[2].pBufferInfo = &brushInfo;
        w[3].dstSet = descriptorSets[i]; w[3].dstBinding = 12; w[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[3].descriptorCount = 1; w[3].pBufferInfo = &quantaInfo;

        vkUpdateDescriptorSets(app->_logicalDevice, (uint32_t)w.size(), w.data(), 0, nullptr);
    }
}

void QuantaSpherePass::CreateGraphicsPipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    auto vertCode = readFile(material.shader.vert_path);
    auto fragCode = readFile(material.shader.frag_path);
    VkShaderModule vs = app->CreateShaderModule(vertCode);
    VkShaderModule fs = app->CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vs; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkViewport vp{ 0,0,(float)app->swapChainExtent.width,(float)app->swapChainExtent.height,0.0f,1.0f };
    VkRect2D sc{ {0,0}, app->swapChainExtent };
    VkPipelineViewportStateCreateInfo vps{};
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1; vps.pViewports = &vp;
    vps.scissorCount  = 1; vps.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = (uint32_t)app->dynamicStates.size();
    dyn.pDynamicStates = app->dynamicStates.data();

    std::array<VkDescriptorSetLayout, 2> setLayouts = {
        app->globalDescriptorSetLayout,
        descriptorSetLayout,
    };

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConsts);

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = (uint32_t)setLayouts.size();
    pli.pSetLayouts = setLayouts.data();
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(app->_logicalDevice, &pli, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("QuantaSpherePass: pipeline layout failed");

    VkFormat colorFmt = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkPipelineRenderingCreateInfo pri{};
    pri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pri.colorAttachmentCount = 1;
    pri.pColorAttachmentFormats = &colorFmt;
    pri.depthAttachmentFormat = app->FindDepthFormat();

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.pNext = &pri;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vin;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vps;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = pipelineLayout;
    if (vkCreateGraphicsPipelines(app->_logicalDevice, VK_NULL_HANDLE, 1, &gp, nullptr, &graphicsPipeline) != VK_SUCCESS)
        throw std::runtime_error("QuantaSpherePass: graphics pipeline failed");

    vkDestroyShaderModule(app->_logicalDevice, vs, nullptr);
    vkDestroyShaderModule(app->_logicalDevice, fs, nullptr);
}

void QuantaSpherePass::Render(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame,
                              VkImageView* /*targetImage*/, UnigmaCameraStruct* CameraMain)
{
    QTDoughApplication* app = QTDoughApplication::instance;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view  = glm::lookAt(CameraMain->position(), CameraMain->position() + CameraMain->forward(), CameraMain->up);
    ubo.proj  = CameraMain->getProjectionMatrix();
    ubo.proj[1][1] *= -1.0f;
    ubo.texelSize = glm::vec2(1.0f / passWidth, 1.0f / passHeight);
    memcpy(_uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

    VkClearValue clearColor{ {{0,0,0,0}} };
    VkClearValue clearDepth{ {1.0f, 0} };

    VkRenderingAttachmentInfo ca{};
    ca.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    ca.imageView = imagesViews[0];
    ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ca.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ca.clearValue = clearColor;

    VkRenderingAttachmentInfo da{};
    da.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    da.imageView = app->depthImageView;
    da.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    da.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    da.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    da.clearValue = clearDepth;

    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea = { {0,0}, app->swapChainExtent };
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &ca;
    ri.pDepthAttachment = &da;

    VkImageMemoryBarrier toAttach{};
    toAttach.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toAttach.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toAttach.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toAttach.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toAttach.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toAttach.image = images[0];
    toAttach.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 };
    toAttach.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toAttach.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0,nullptr, 0,nullptr, 1, &toAttach);

    vkCmdBeginRendering(cmd, &ri);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkDescriptorSet sets[2] = {
        app->globalDescriptorSets[currentFrame],
        descriptorSets[currentFrame],
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);

    qpc.quantaCount = QUANTA_COUNT;
    vkCmdPushConstants(cmd, pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConsts), &qpc);

    VkViewport vp{ 0,0,(float)app->swapChainExtent.width,(float)app->swapChainExtent.height,0,1 };
    VkRect2D sc{ {0,0}, app->swapChainExtent };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    vkCmdDraw(cmd, 4, QUANTA_COUNT, 0, 0);

    vkCmdEndRendering(cmd);

    VkImageMemoryBarrier toRead = toAttach;
    toRead.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0,nullptr, 0,nullptr, 1, &toRead);
}
