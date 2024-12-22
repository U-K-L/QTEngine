#include "QTDoughApplication.h"

#include "../Engine/Renderer/UnigmaRenderingManager.h"
#include "../Engine/Camera/UnigmaCamera.h"
#include "../Engine/RenderPasses/RenderPassObject.h"
#include "../Engine/RenderPasses/BackgroundPass.h"
#include "../Engine/RenderPasses/CompositionPass.h"
#include "../Engine/RenderPasses/AlbedoPass.h"
#include "../Engine/RenderPasses/NormalPass.h"
#include "../Engine/RenderPasses/PositionPass.h"
#include "../UnigmaNative/UnigmaNative.h"
#include "stb_image.h"
UnigmaRenderingObject unigmaRenderingObjects[NUM_OBJECTS];
UnigmaCameraStruct CameraMain;
std::vector<RenderPassObject*> renderPassStack;
QTDoughApplication* QTDoughApplication::instance = nullptr;
std::unordered_map<std::string, UnigmaTexture> textures;

bool PROGRAMEND = false;
uint32_t currentFrame = 0;

bool initialStart = false;
//extern SDL_Window *SDLWindow;

void QTDoughApplication::AddRenderObject(UnigmaRenderingStructCopyableAttributes* renderObject, UnigmaGameObject* gObj, uint32_t index)
{
    unigmaRenderingObjects[gObj->RenderID] = *renderObject;
    unigmaRenderingObjects[gObj->RenderID].isRendering = true;
    unigmaRenderingObjects[gObj->RenderID]._material = renderObject->_material;
    //Set ID of renderer
    unigmaRenderingObjects[gObj->RenderID]._renderer.GID = gObj->RenderID;


    for (auto& [key, value] : unigmaRenderingObjects[gObj->RenderID]._material.vectorProperties) {
        std::cout << key << " : " << value.x << " " << value.y << " " << value.z << " " << value.w << std::endl;
    }

}

void QTDoughApplication::UpdateObjects(UnigmaRenderingStruct* renderObject, UnigmaGameObject* gObj, uint32_t index)
{
    //if(initialStart == false) //remove this later.
    CameraMain = *UNGetCamera(0);
    //initialStart = true;
    //print FOV.
    //UnigmaCameraStruct cam = UnigmaCameraStruct();
    //std::cout << "Camera FOV: " << CameraMain.fov << std::endl;

    unigmaRenderingObjects[gObj->RenderID]._transform = gObj->transform.position;
}

int QTDoughApplication::Run() {

    CameraMain = UnigmaCameraStruct();
    InitSDLWindow();
	InitVulkan();

    SDL_Event e; 
    while (PROGRAMEND == false) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                PROGRAMEND = true;
            }
            else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    framebufferResized = true;
                }
            }
            ImGui_ImplSDL2_ProcessEvent(&e);
        }
        // Main Game Loop.
        RunMainGameLoop();
    }

    vkDeviceWaitIdle(_logicalDevice);
	return 0;
}


void QTDoughApplication::RunMainGameLoop()
{
    //Get current time.
    currentTime = std::chrono::high_resolution_clock::now();
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Performance"); // Create a window titled "Performance"

    // Display FPS
    if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeSecondPassed).count() > 999)
    {
        ImGuiIO& io = ImGui::GetIO();
        FPS = io.Framerate;
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / FPS, FPS);

    ImGui::End(); // End the ImGui window

    //make imgui calculate internal draw structures
    ImGui::Render();

    DrawFrame();
    if (GatherBlenderInfo() == 0)
    {
        CameraToBlender();
        GetMeshDataAllObjects();
    }
    //Set new data.
    
    //Quit if E key is pressed.
    const Uint8* state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_E]) {
        // The "E" key is currently pressed
        PROGRAMEND = true;
    }


    // Calculate the elapsed time in milliseconds
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeSecondPassed);

    if (elapsedTime.count() >= 1000) {

        timeSecondPassed = currentTime;
    }

    elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeMinutePassed);

    if (elapsedTime.count() >= 1000 * 60) {

        timeMinutePassed = currentTime;
    }

    //RecreateResources();
}

void QTDoughApplication::DrawFrame()
{
    //Waits for this fence to finish. 
    vkWaitForFences(_logicalDevice, 1, &_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    //Aquire the rendered image.
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_logicalDevice, _swapChain, UINT64_MAX, _imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    //Resize screen if something had changed.
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    UpdateGlobalDescriptorSet();

    //Set fence back to unsignled for next time.
    vkResetFences(_logicalDevice, 1, &_inFlightFences[currentFrame]);

    //REturn command buffers back to original state.
    vkResetCommandBuffer(_commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordCommandBuffer(_commandBuffers[currentFrame], imageIndex);



    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    //Sends the recorded command buffer to be rendered.
    if (vkQueueSubmit(_vkGraphicsQueue, 1, &submitInfo, _inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { _swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        RecreateSwapChain();
        UpdateGlobalDescriptorSet();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void QTDoughApplication::InitSDLWindow()
{
    //The surface contained by the window
    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    }
    else
    {
        //Create window
        QTSDLWindow = SDL_CreateWindow("QTDough", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (QTSDLWindow == NULL)
        {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        }
        else
        {
            //Get window surface
            _screenSurface = SDL_GetWindowSurface(QTSDLWindow);

            //Fill the surface white
            SDL_FillRect(_screenSurface, NULL, SDL_MapRGB(_screenSurface->format, 0x00, 0x00, 0x00));

            //Update the surface
            SDL_UpdateWindowSurface(QTSDLWindow);

            printf("Window Created!!!\n");

            //Hack to get window to stay up
            //SDL_Event e; bool quit = false; while (quit == false) { while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; } }


        }
    }
}

VkVertexInputBindingDescription QTDoughApplication::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> QTDoughApplication::getAttributeDescriptions() {

    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, normal);

    return attributeDescriptions;
}

bool QTDoughApplication::HasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat QTDoughApplication::FindDepthFormat() {
    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat QTDoughApplication::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");

}

VkRenderingAttachmentInfo QTDoughApplication::AttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;

    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (clear) {
        colorAttachment.clearValue = *clear;
    }

    return colorAttachment;
}

VkRenderingInfo QTDoughApplication::RenderingInfo(VkExtent2D extent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment)
{
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.pNext = nullptr;
    renderingInfo.flags = 0;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = 0; // For multiview rendering, if needed

    if (colorAttachment != nullptr)
    {
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = colorAttachment;
    }
    else
    {
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pColorAttachments = nullptr;
    }

    renderingInfo.pDepthAttachment = depthAttachment;
    renderingInfo.pStencilAttachment = nullptr; // Assuming no separate stencil attachment

    return renderingInfo;
}


void QTDoughApplication::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = AttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = RenderingInfo(swapChainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

VkSubmitInfo QTDoughApplication::SubmitInfo(VkCommandBuffer* cmd)
{
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext = nullptr;

    info.waitSemaphoreCount = 0;
    info.pWaitSemaphores = nullptr;
    info.pWaitDstStageMask = nullptr;
    info.commandBufferCount = 1;
    info.pCommandBuffers = cmd;
    info.signalSemaphoreCount = 0;
    info.pSignalSemaphores = nullptr;

    return info;
}

VkSubmitInfo2 QTDoughApplication::SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

VkCommandBufferSubmitInfo QTDoughApplication::CommandBufferSubmitInfo(VkCommandBuffer cmd)
{
    VkCommandBufferSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.pNext = nullptr;
    info.commandBuffer = cmd;
    info.deviceMask = 0;

    return info;
}

VkCommandBufferBeginInfo QTDoughApplication::CommandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

VkCommandBufferAllocateInfo QTDoughApplication::CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    return info;
}

void QTDoughApplication::InitCommands()
{
    VK_CHECK(vkCreateCommandPool(_logicalDevice, &_commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &cmdAllocInfo, &_immCommandBuffer));


    /*
    _mainDeletionQueue.push_function([=]() {
        vkDestroyCommandPool(_logicalDevice, _immCommandPool, nullptr);
        });
    */

}

void QTDoughApplication::InitSyncStructures()
{
    VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_immFence));

}

void QTDoughApplication::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_logicalDevice, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = CommandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submit = SubmitInfo(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_vkGraphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_logicalDevice, 1, &_immFence, true, 9999999999));
}

void QTDoughApplication::InitImGui()
{
    //1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_logicalDevice, &pool_info, nullptr, &imguiPool));

    std::cout << "Started pool info imgui" << std::endl;
    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(QTSDLWindow);
    std::cout << "Started pool info imgui" << std::endl;
    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _vkInstance;
    init_info.PhysicalDevice = _physicalDevice;
    init_info.Device = _logicalDevice;
    init_info.Queue = _vkGraphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapChainImageFormat;


    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    /*
    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        });
        */
}

uint32_t QTDoughApplication::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }


    throw std::runtime_error("failed to find suitable memory type!");
}

void QTDoughApplication::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(_logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_logicalDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(_logicalDevice, buffer, bufferMemory, 0);
}

/*
void QTDoughApplication::UpdateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}
*/
void QTDoughApplication::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);
}

void QTDoughApplication::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();



    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    EndSingleTimeCommands(commandBuffer);
}

void QTDoughApplication::AddPasses()
{

    CompositionPass* compPass = new CompositionPass();
    PositionPass* positionPass = new PositionPass();
    NormalPass* normalPass = new NormalPass();
    AlbedoPass* albedoPass = new AlbedoPass();
    BackgroundPass* bgPass = new BackgroundPass();
    //Add objects.
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
        {
            albedoPass->renderingObjects.push_back(&unigmaRenderingObjects[i]);
            normalPass->renderingObjects.push_back(&unigmaRenderingObjects[i]);
            positionPass->renderingObjects.push_back(&unigmaRenderingObjects[i]);
        }
    }

    renderPassStack.push_back(bgPass);
    renderPassStack.push_back(albedoPass);
    renderPassStack.push_back(normalPass);
    renderPassStack.push_back(positionPass);
    renderPassStack.push_back(compPass);





    std::cout << "Passes count: " << renderPassStack.size() << std::endl;
}

void QTDoughApplication::InitVulkan()
{

    //Create the intial instances, windows, get the GPU and create the swap chain.
	CreateInstance();
    CreateWindowSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();

    AddPasses();

    //Create all the image views.
    CreateImageViews();

    //The descriptor layouts.
    CreateDescriptorSetLayout();
    CreateGlobalDescriptorSetLayout();
    //CreateOffscreenDescriptorSetLayout();

    //Create graphics pipelines.
    CreateMaterials();
    CreateGraphicsPipeline();
    //CreateBackgroundGraphicsPipeline();

    //Create Depths
    CreateDepthResources();
    CreateCommandPool();

    //Create the images and their samplers.
    //CreateTextureImage();
    //CreateTextureImageView();
    CreateTextureSampler();
    CreateImages();

    //Load models and create the buffers for vertices and indicies and uniforms.
    //LoadModel();
    CreateVertexBuffer();
    CreateQuadBuffers();
    CreateIndexBuffer();
    CreateUniformBuffers();

    //Create descriptor pools and sets
    CreateDescriptorPool();
    CreateGlobalDescriptorPool();
    CreateDescriptorSets();
    CreateGlobalDescriptorSet();

    //Create command buffers.
    CreateCommandBuffers();

    //Imgui.
    InitImGui();

    //Sync the buffers.
    CreateSyncObjects();

}

void QTDoughApplication::CreateImages()
{

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateImages();
    }

}

void QTDoughApplication::GetMeshDataAllObjects()
{
    //Loop over all objects
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        //unigmaRenderingObjects[i].RenderObjectToUnigma(*this, renderObjects[i], unigmaRenderingObjects[i], CameraMain);

        unigmaRenderingObjects[i].isRendering = false;
        if (strstr(renderObjects[i].name, "Camera") != nullptr)
            continue;
        if (strstr(renderObjects[i].name, "Light") != nullptr)
            continue;
        if (strstr(renderObjects[i].name, "Cube") != nullptr)
        {
            unigmaRenderingObjects[i].isRendering = true;
            unigmaRenderingObjects[i].RenderObjectToUnigma(*this, renderObjects[i], unigmaRenderingObjects[i], CameraMain);
        }

    }
}

void QTDoughApplication::CreateVertexBuffer()
{
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateVertexBuffer(*this);
    }
}

void QTDoughApplication::CreateIndexBuffer()
{
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateIndexBuffer(*this);
    }
}


void QTDoughApplication::LoadModel()
{
    unigmaRenderingObjects[0].LoadModel(unigmaRenderingObjects[0]._mesh);
}

void QTDoughApplication::CreateDepthResources()
{
    VkFormat depthFormat = FindDepthFormat();

    CreateImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = CreateImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);


}

void QTDoughApplication::CreateTextureSampler() {

    vkGetPhysicalDeviceProperties(_physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(_logicalDevice, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

}



void QTDoughApplication::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    EndSingleTimeCommands(commandBuffer);
}

VkCommandBuffer QTDoughApplication::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void QTDoughApplication::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(_logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(_logicalDevice, image, imageMemory, 0);
}

void QTDoughApplication::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vkGraphicsQueue);

    vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &commandBuffer);
}

void QTDoughApplication::CreateTextureImage()
{
    /*
    //std::string path = AssetsPath + unigmaRenderingObjects[0]._material.textures[0].TEXTURE_PATH;
    std::string path = AssetsPath + "Textures/viking_room.png";
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(_logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(_logicalDevice, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(_logicalDevice, stagingBufferMemory, nullptr);
    */
}

void QTDoughApplication::CreateDescriptorSets()
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateDescriptorSets();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateDescriptorSets(*this);
    }
}

void QTDoughApplication::CreateDescriptorPool()
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateDescriptorPool();
    }
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateDescriptorPool(*this);
    }
}

void QTDoughApplication::CreateUniformBuffers()
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateUniformBuffers();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateUniformBuffers(*this);
    }
}

void QTDoughApplication::CreateDescriptorSetLayout()
{

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateDescriptorSetLayout();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateDescriptorSetLayout(*this);
    }
}

void QTDoughApplication::CreateGlobalDescriptorSetLayout()
{
    CreateGlobalSamplers(1000);

    // Binding for sampled images
    VkDescriptorSetLayoutBinding sampledImageBinding{};
    sampledImageBinding.binding = 0;
    sampledImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sampledImageBinding.descriptorCount = 1000;
    sampledImageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    sampledImageBinding.pImmutableSamplers = nullptr;

    // Binding for samplers
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerBinding.descriptorCount = 1000;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = globalSamplers.data();

    // We now have two bindings
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { sampledImageBinding, samplerBinding };

    // For descriptor indexing flags
    VkDescriptorBindingFlags bindingFlags[2] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pBindings = bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pNext = &bindingFlagsInfo;

    // Enable update-after-bind for descriptor indexing
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    if (vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr, &globalDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create global descriptor set layout!");
    }
}

void QTDoughApplication::CreateGlobalDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = 1000;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = 1000;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    if (vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &globalDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create global descriptor pool!");
    }
}

void QTDoughApplication::UpdateGlobalDescriptorSet()
{
    std::vector<UnigmaTexture> keys;
    int index = 0;
    for (auto& pair : textures) {
        pair.second.ID = index;
        keys.push_back(pair.second);
        index++;
    }

    auto sizeTextures = keys.size();
    std::vector<VkDescriptorImageInfo> imageInfos(sizeTextures);

    for (size_t i = 0; i < sizeTextures; ++i) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = keys[i].u_imageView;
        imageInfos[i].sampler = VK_NULL_HANDLE; // Samplers handled separately
    }

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = globalDescriptorSet;
    imageWrite.dstBinding = 0; // Binding for sampled images
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageWrite.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    imageWrite.pImageInfo = imageInfos.data();

    vkUpdateDescriptorSets(_logicalDevice, 1, &imageWrite, 0, nullptr);
}


void QTDoughApplication::LoadTexture(const std::string& filename) {
    if (textures.count(filename) > 0) {
        return;
    }

    UnigmaTexture texture;

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, 4);
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(_logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(_logicalDevice, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(
        texWidth, texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        texture.u_image,
        texture.u_imageMemory
    );


    // Initial layout transition
    TransitionImageLayout(
        texture.u_image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    std::cout << "Texture Loaded Name: " << filename << std::endl;
    CopyBufferToImage(stagingBuffer, texture.u_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    // Final layout transition
    TransitionImageLayout(
        texture.u_image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(_logicalDevice, stagingBufferMemory, nullptr);

    texture.u_imageView = CreateImageView(texture.u_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    std::vector<std::string> tokens;
    std::istringstream tokenStream(filename);
    std::string token;
    while (std::getline(tokenStream, token, '/')) {
        tokens.push_back(token);
    }

    // Safely remove .png if present
    if (!tokens.empty()) {
        std::string& lastToken = tokens.back();
        if (lastToken.size() >= 4 && lastToken.compare(lastToken.size() - 4, 4, ".png") == 0) {
            lastToken.erase(lastToken.end() - 4, lastToken.end());
        }
        std::cout << "Texture Loaded Name Final Token: " << tokens.back() << std::endl;
    }

    std::string textureName = tokens.empty() ? filename : tokens.back();
    textures.insert({ textureName, texture });
}




void QTDoughApplication::CreateGlobalDescriptorSet()
{
    uint32_t maxDescriptorCount = 1000;

    VkDescriptorSetVariableDescriptorCountAllocateInfo descriptorCountAllocInfo{};
    descriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    descriptorCountAllocInfo.descriptorSetCount = 1;
    descriptorCountAllocInfo.pDescriptorCounts = &maxDescriptorCount;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = globalDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &globalDescriptorSetLayout;
    allocInfo.pNext = &descriptorCountAllocInfo;

    if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &globalDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate global descriptor set!");
    }
}


void QTDoughApplication::CreateSyncObjects()
{
    //Resize to fit the amount of possible frames in flight.
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);


    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //First frame doesn't wait.

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}


void QTDoughApplication::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Render all your passes
    RenderPasses(commandBuffer, imageIndex);

    // Now transition the depth image from DEPTH_STENCIL_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = depthImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }


    DrawImgui(commandBuffer, swapChainImageViews[imageIndex]);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void QTDoughApplication::CameraToBlender()
{
    if (renderObjectsMap.count("OBCamera") > 0)
    {
        CameraMain._transform = renderObjectsMap["OBCamera"]->transformMatrix;
    }
}

void QTDoughApplication::RenderPasses(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->UpdateUniformBuffer(imageIndex, currentFrame);
        //renderPassStack[i]->UpdateUniformBufferObjects(commandBuffer, imageIndex, currentFrame, nullptr, &CameraMain);
    }

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->Render(commandBuffer, imageIndex, currentFrame, nullptr, &CameraMain);
    }
}

void QTDoughApplication::RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
        {
            //print out is rendering.
            std::cout << "Rendering Object: " << unigmaRenderingObjects[i].isRendering << std::endl;
            //unigmaRenderingObjects[i].UpdateUniformBuffer(*this, currentFrame, unigmaRenderingObjects[i], CameraMain);
            //unigmaRenderingObjects[i].Render(*this, commandBuffer, imageIndex, currentFrame);
        }
    }
}

void QTDoughApplication::CreateCommandBuffers()
{
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();


    if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    std::cout << "Created Command Buffer" << std::endl;
}

void QTDoughApplication::CreateCommandPool()
{
    std::cout << "Creating Command Pool" << std::endl;
    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(_physicalDevice);
    _commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    _commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    _commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(_logicalDevice , &_commandPoolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    std::cout << "Created Command Pool" << std::endl;
}

void QTDoughApplication::CreateRenderPass()
{
    std::cout << "Creating Render Passes" << std::endl;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;


    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;


    std::cout << "Subpasses created." << std::endl;

    if (vkCreateRenderPass(_logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    std::cout << "Renderpasses created." << std::endl;
}


//Graphics pipeline has to be made per shader, and therefore per material using a different shader.
void QTDoughApplication::CreateGraphicsPipeline()
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateGraphicsPipeline();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if(unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateGraphicsPipeline(*this);
    }
}

VkShaderModule QTDoughApplication::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void QTDoughApplication::CreateTextureImageView() {
    textureImageView = CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void QTDoughApplication::CreateGlobalSamplers(uint32_t samplerCount)
{
    // Define common sampler settings
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_TRUE;           
    samplerInfo.maxAnisotropy = 16.0f;               
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;   
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;       
    samplerInfo.maxLod = 0.0f;
    samplerInfo.mipLodBias = 0.0f;

    globalSamplers.resize(samplerCount);

    for (uint32_t i = 0; i < samplerCount; ++i) {
        if (vkCreateSampler(_logicalDevice, &samplerInfo, nullptr, &globalSamplers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create sampler!");
        }
    }
}

VkImageView QTDoughApplication::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(_logicalDevice , &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void QTDoughApplication::CreateImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = CreateImageView(swapChainImages[i], _swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void QTDoughApplication::CreateMaterials() {
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateMaterials();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if(unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateGraphicsPipeline(*this);
    }
}

void QTDoughApplication::CreateSwapChain() {

    std::cout << "Creating swap chain" << std::endl;
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _vkSurface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(_logicalDevice , _swapChain, &imageCount, nullptr);
    std::cout << "Created swap chain" << std::endl;
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, swapChainImages.data());

    _swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void QTDoughApplication::CreateInstance()
{
    unsigned int extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    VkApplicationInfo appInfo{};
    VkInstanceCreateInfo createInfo{};
    
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL); //Get the count.
    const char** extensions = (const char**)malloc(sizeof(char*) * extensionCount); //Well justified ;p
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, extensions); //Get the extensions.

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "QTDough";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "QTDoughEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 0;

    auto sdlExtensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(sdlExtensions.size());
    createInfo.ppEnabledExtensionNames = sdlExtensions.data();

    if (enableValidationLayers && !CheckValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    //Finally create the instance.
    VkResult result = vkCreateInstance(&createInfo, nullptr, &_vkInstance);
    if (vkCreateInstance(&createInfo, nullptr, &_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

}

bool QTDoughApplication::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void QTDoughApplication::PickPhysicalDevice()
{
    
    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, nullptr);

    std::cout << "Device count: " << deviceCount << std::endl;

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            _physicalDevice = device;
            break;
        }
    }

    if (_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU! Ensure you have GPU that supports Ray Tracing Acceleration");
    }

    std::cout << "Suitable device found" << std::endl;
}

bool QTDoughApplication::IsDeviceSuitable(VkPhysicalDevice device) {

    //Set up device features / properties struct.
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    //After getting GPU features create list of features we want GPU to have to run program.

    //Must support raytracing. Remove this later, adding this for now due to testing....
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
    rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &rayTracingFeatures;
    
    //Get the features of this device. Does it support ray tracing?
    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);
    QueueFamilyIndices indices = FindQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    //return indices.isComplete() && extensionsSupported && swapChainAdequate;

    //return true;

    return rayTracingFeatures.rayTracingPipeline &&
           indices.graphicsFamily.has_value();
}

std::vector<const char*> QTDoughApplication::GetRequiredExtensions() {
    uint32_t extensionCount = 0;

    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL);
    const char** sdlExtensions = (const char**)malloc(sizeof(char*) * extensionCount);
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, sdlExtensions);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}


QueueFamilyIndices QTDoughApplication::FindQueueFamilies(VkPhysicalDevice device) {

    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _vkSurface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

void QTDoughApplication::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    _createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = queueCreateInfos.data();

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;

    queueCreateInfo.pQueuePriorities = &queuePriority;

    _createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    VkPhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    vkGetPhysicalDeviceFeatures(_physicalDevice, &deviceFeatures);

    _createInfo.pQueueCreateInfos = &queueCreateInfo;
    _createInfo.queueCreateInfoCount = 1;

    _createInfo.pEnabledFeatures = &deviceFeatures;

    _createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    _createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        _createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        _createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        _createInfo.enabledLayerCount = 0;
    }


    // Bindless Texture support.
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;
    deviceFeatures2.pNext = &descriptorIndexingFeatures;

    _createInfo.pNext = &deviceFeatures2;



    //Finally instantiate this device.
    if (vkCreateDevice(_physicalDevice, &_createInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }


    vkGetDeviceQueue(_logicalDevice, indices.graphicsFamily.value(), 0, &_vkGraphicsQueue);

    vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue);
}

void QTDoughApplication::CleanupSwapChain()
{
    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(_logicalDevice, swapChainFramebuffers[i], nullptr);
    }

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(_logicalDevice, swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
}

void QTDoughApplication::RecreateSwapChain()
{
    int width = 0, height = 0;
    SDL_GetWindowSize(QTSDLWindow, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(QTSDLWindow, &width, &height);
        SDL_WaitEvent(NULL);
    }

    vkDeviceWaitIdle(_logicalDevice);

    CleanupSwapChain();


    // Recreate swapchain and dependent resources
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();

    RecreateResources();

    CreateCommandBuffers();
}

void QTDoughApplication::RecreateResources()
{
    /*
    vkDeviceWaitIdle(_logicalDevice);

    vkDestroyDescriptorPool(_logicalDevice, globalDescriptorPool, nullptr);

    // Recreate pool
    CreateGlobalDescriptorPool();

    // Allocate sets again
    CreateGlobalDescriptorSet(); // Or call vkAllocateDescriptorSets again
    */
    /*
    // Destroy all textures and associated memory
    for (auto& pair : textures) {
        UnigmaTexture& tex = pair.second;
        vkDestroyImageView(_logicalDevice, tex.u_imageView, nullptr);
        vkDestroyImage(_logicalDevice, tex.u_image, nullptr);
        vkFreeMemory(_logicalDevice, tex.u_imageMemory, nullptr);
    }
    */
    QTDoughApplication::instance->textures.clear();


    //CreateSwapChain();
    //CreateImageViews();

    //UpdateGlobalDescriptorSet(); // Update descriptors with the newly loaded textures

    // Recreate graphics pipelines for each render pass
    for (auto& renderPass : renderPassStack) {
        renderPass->CleanupPipeline();
        renderPass->CreateMaterials();
        renderPass->CreateGraphicsPipeline();
    }

    CreateImages();

    UpdateGlobalDescriptorSet();
}


void QTDoughApplication::CreateWindowSurface()
{

    if (!SDL_Vulkan_CreateSurface(QTSDLWindow, _vkInstance, &_vkSurface)) {
        throw std::runtime_error("Failed to create window surface!");
    }
    std::cout << "Created Window Surface" << std::endl;


    /*
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    _createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = queueCreateInfos.data();

    vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue);
    */
}


bool QTDoughApplication::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails QTDoughApplication::QuerySwapChainSupport(VkPhysicalDevice device) {
    std::cout << "Swap chain being queried" << std::endl;
    SwapChainSupportDetails details;

    VkResult result;

    // Get surface capabilities
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _vkSurface, &details.capabilities);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface capabilities: " << result << std::endl;
        // Handle the error appropriately (e.g., return, throw exception)
        return details;
    }

    // Get surface formats
    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        std::cerr << "Failed to get physical device surface formats: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    details.formats.resize(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, details.formats.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface formats data: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    // Get present modes
    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        std::cerr << "Failed to get physical device surface present modes: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    details.presentModes.resize(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, details.presentModes.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface present modes data: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    std::cout << "Swap chain support details acquired successfully." << std::endl;
    return details;
}

VkSurfaceFormatKHR QTDoughApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR QTDoughApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}


VkExtent2D QTDoughApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {

    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        SDL_GetWindowSize(QTSDLWindow, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }

}


void QTDoughApplication::CreateQuadBuffers() {

    std::vector<Vertex> quadVertices = {
        {.pos = { -1.0f, -1.0f, 0.0f }, .texCoord = { 0.0f, 1.0f } },
        {.pos = { 1.0f, -1.0f, 0.0f }, .texCoord = { 1.0f, 1.0f } },
        {.pos = { 1.0f, 1.0f, 0.0f }, .texCoord = { 1.0f, 0.0f } },
        {.pos = { -1.0f, 1.0f, 0.0f }, .texCoord = { 0.0f, 0.0f } }
    };

    std::vector<uint16_t> quadIndices = {
        0, 1, 2, // First triangle
        2, 3, 0  // Second triangle
    };



    VkDeviceSize bufferSize = sizeof(quadVertices[0]) * quadVertices.size();

    // Create vertex buffer
    CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        quadVertexBuffer,
        quadVertexBufferMemory
    );

    // Copy vertex data
    void* data;
    vkMapMemory(_logicalDevice, quadVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, quadVertices.data(), (size_t)bufferSize);
    vkUnmapMemory(_logicalDevice, quadVertexBufferMemory);

    // Create index buffer
    bufferSize = sizeof(quadIndices[0]) * quadIndices.size();

    CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        quadIndexBuffer,
        quadIndexBufferMemory
    );

    // Copy index data
    vkMapMemory(_logicalDevice, quadIndexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, quadIndices.data(), (size_t)bufferSize);
    vkUnmapMemory(_logicalDevice, quadIndexBufferMemory);
}



void QTDoughApplication::Cleanup()
{
    CleanupSwapChain();

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        unigmaRenderingObjects[i].Cleanup(*this);
    }

    vkDestroyImageView(_logicalDevice, depthImageView, nullptr);
    vkDestroyImage(_logicalDevice, depthImage, nullptr);
    vkFreeMemory(_logicalDevice, depthImageMemory, nullptr);

    vkDestroySampler(_logicalDevice, textureSampler, nullptr);
    vkDestroyImageView(_logicalDevice, textureImageView, nullptr);

    vkDestroyImage(_logicalDevice, textureImage, nullptr);
    vkFreeMemory(_logicalDevice, textureImageMemory, nullptr);


    vkDestroyImageView(_logicalDevice, textureImageView, nullptr);

    vkDestroyImage(_logicalDevice, textureImage, nullptr);
    vkFreeMemory(_logicalDevice, textureImageMemory, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
    }
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(_logicalDevice, imageView, nullptr);
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(_logicalDevice, _renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(_logicalDevice, _inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);
    vkDestroyPipeline(_logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);
    vkDestroyRenderPass(_logicalDevice, renderPass, nullptr);
    vkDestroyInstance(_vkInstance, nullptr);
    vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
    vkDestroyDevice(_logicalDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    SDL_DestroyWindow(QTSDLWindow);
    SDL_Quit();
}