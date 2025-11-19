#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define NUM_OBJECTS 16384
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include <optional>
#include <cstdint> 
#include <limits> 
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../Engine/Renderer/UnigmaLights.h"
#include "../UnigmaNative/UnigmaThread.h"

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_system.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <stdexcept>
#include <set>
#include <stack>
#include "UnigmaBlend.h"
#include "../Engine/Renderer/UnigmaTexture.h"
#include "../Engine/Renderer/UnigmaRenderingStruct.h"
#include "../Engine/Core/UnigmaGameObject.h"

#include <array>
#include <chrono>
#include <map>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include <functional>
#include "VideoRecorder.h"





#define VK_CHECK(x)                                                         \
    do {                                                                    \
        VkResult err = x;                                                   \
        if (err) {                                                          \
            std::cerr << "Detected Vulkan error: " << err << std::endl;     \
            std::abort();                                                   \
        }                                                                   \
    } while (0)

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

static const std::string AssetsPath = "Assets/";

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

struct GPULight
{
    glm::vec4 emission;
    glm::vec3 direction;
};

struct GlobalUniformBufferObject
{
    float deltaTime;    // offset 0
    float time;         // offset 4
    float pad[2];       // offset 8..15 to make total 16 bytes
    GPULight light[32];
    // total struct size = 16 bytes
};

struct GameObjectShaderData
{
    glm::mat4 transform;
    glm::vec4 BaseAlbedo;
    glm::vec4 TopAlbedo;
    glm::vec4 SideAlbedo;

};

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}





#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

//QTDough Class.
class QTDoughApplication {
public:
    QTDoughApplication() {}

    static void SetInstance(QTDoughApplication* app)
    {
        instance = app;
    }
    static QTDoughApplication* instance;
    QTDoughApplication(const QTDoughApplication&) = delete;
    QTDoughApplication& operator=(const QTDoughApplication&) = delete;
    UnigmaThread* QTDoughEngineThread;
    //Methods.
    int Run();
    void Cleanup();
    void AddPasses();
    void UpdateObjects(UnigmaRenderingStruct* renderObject, UnigmaGameObject* gObj, uint32_t index);
    void AddRenderObject(UnigmaRenderingStructCopyableAttributes* renderObject, UnigmaGameObject* gObj, uint32_t index);
    void ClearObjectData();
    void CreateGlobalUniformBuffers();
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    //Fields.
    bool PROGRAMEND = false;
    std::chrono::high_resolution_clock::time_point timeSinceApplication;
    std::chrono::high_resolution_clock::time_point timeSecondPassed;
    std::chrono::high_resolution_clock::time_point timeMinutePassed;
    std::chrono::high_resolution_clock::time_point currentTime;
    std::vector<VkBuffer> globalUniformBufferObject;
    std::vector<VkDeviceMemory> globalUniformBuffersMemory;
    std::vector<VkDescriptorSet> globalDescriptorSets;
    std::vector<VkFence> computeInFlightFences;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    VkDebugUtilsMessengerEXT debugMessenger;
    uint32_t currentFrame;
    float FPS;
    SDL_Window* QTSDLWindow;
    int SCREEN_WIDTH = 1280;
    int SCREEN_HEIGHT = 720;
    int MAX_BINDLESS_IMAGES = 10000;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkExtent2D swapChainExtent;
    VkRenderPass renderPass;
    VkPipelineLayout _pipelineLayout;
    VkPipeline graphicsPipeline;
    VkDevice _logicalDevice = VK_NULL_HANDLE;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    VkImageView textureImageView;
    VkSampler textureSampler;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkFormat _swapChainImageFormat;
    SDL_Surface* _screenSurface = NULL;
    std::vector<VkFence> computeFences;
    bool framebufferResized = false;
    VkFormat FindDepthFormat();
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    VkVertexInputBindingDescription getBindingDescription();
    std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();
    const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
    };
    std::unordered_map<std::string, UnigmaTexture> textures;
    std::map<std::string, Unigma3DTexture> textures3D;
    std::vector<UnigmaLight*> lights;

    //Quads
    VkBuffer quadVertexBuffer;
    VkDeviceMemory quadVertexBufferMemory;
    VkBuffer quadIndexBuffer;
    VkDeviceMemory quadIndexBufferMemory;
    VideoRecorder* recorder = nullptr;

    VkSampleCountFlagBits GetMaxUsableSampleCount();
    void TransitionOffscreenImagesForSampling(VkCommandBuffer commandBuffer);
    void TransitionOffscreenImagesForRendering(VkCommandBuffer commandBuffer);
    void CreateQuadBuffers();
    void CreateImages();
    void CreateMaterials();
    void CreateOffscreenDescriptorSetLayout();
    void CreateCompositionDescriptorSet();
    void CreateCompositionDescriptorPool();
    void CreateCompositionPipeline();
    void CreateGlobalDescriptorPool();
    void CreateGlobalDescriptorSet();
    void CreateGlobalDescriptorSetLayout();
    void UpdateGlobalDescriptorSet();
    //void DebugPrintParticles(uint32_t currentFrame);
    void LoadTexture(const std::string& filename);
    void CompositePass(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
    void RecreateResources();
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkFence fence);
    void EndSingleTimeCommandsAsync(uint32_t currentFrame, VkCommandBuffer commandBuffer, std::function<void()> callback);
    void CreateGlobalSamplers(uint32_t samplerCount);
    void ReadbackBufferData(VkBuffer srcBuffer, VkDeviceSize size, void* pDstData, VkDeviceSize srcOffset);

    void StartRecording(const char* path, uint32_t fps);
    void StopRecording();

    void CreateImages3D(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView Create3DImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkImage albedoImage;
    VkDeviceMemory albedoImageMemory;
    VkImageView albedoImageView;

    VkDescriptorPool descriptorPool;
    //Composite.
    VkDescriptorSetLayout compositionDescriptorSetLayout;
    VkDescriptorPool compositionDescriptorPool;
    VkDescriptorSet compositionDescriptorSet;

    //Bindless
    VkDescriptorSetLayout globalDescriptorSetLayout;
    VkDescriptorPool globalDescriptorPool;
    //VkDescriptorSet globalDescriptorSet;
    std::vector<VkSampler> globalSamplers;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;



private:

    //Methods.
    void InitSDLWindow();
    void InitVulkan();
    void SetupDebugMessenger();
    void MainLoop();
    void CreateInstance();
    bool CheckValidationLayerSupport();
    void PickPhysicalDevice();
    bool IsDeviceSuitable(VkPhysicalDevice device);
    void CreateLogicalDevice();
    void CreateWindowSurface();
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    void CreateSwapChain();
    void CreateImageViews();
    void CreateGraphicsPipeline();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateCompute();
    void CreateShaderStorageBuffers();
    void RunMainGameLoop();
    void DrawFrame();
    void CreateSyncObjects();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateDescriptorSetLayout();
    void CreateTextureImage();
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    std::vector<const char*> GetRequiredExtensions();
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    VkRenderingAttachmentInfo AttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);
    void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void RecordComputeCommandBuffer(VkCommandBuffer commandBuffer);
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void InitCommands();
    void InitSyncStructures();
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateUniformBuffers();
    void UpdateUniformBuffer(uint32_t currentImage);
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateTextureSampler();
    void RecreateSwapChain();
    void CleanupSwapChain();
    void InitImGui();
    void CreateTextureImageView();
    void CreateDepthResources();
    bool HasStencilComponent(VkFormat format);
    void LoadModel();
    void RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void RenderPasses(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void GetMeshDataAllObjects();
    void CameraToBlender();
    void CreateComputeCommandBuffers();
    void CreateComputeDescriptorSetLayout();
    void CreateComputeDescriptorSets();
    void CreateComputePipeline();
    void DispatchPasses(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void DebugCompute(uint32_t currentFrame);


    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/);
    VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/);
    VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);
    VkSubmitInfo SubmitInfo(VkCommandBuffer* cmd);
    VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
        VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkRenderingInfo RenderingInfo(
        VkExtent2D extent,
        VkRenderingAttachmentInfo* colorAttachment,
        VkRenderingAttachmentInfo* depthAttachment);
    //Fields.
    VkInstance _vkInstance;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkQueue _vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue _vkComputeQueue = VK_NULL_HANDLE;
    VkSurfaceKHR _vkSurface = VK_NULL_HANDLE;

    std::vector<VkBuffer> globalShaderGameObjs;
    std::vector<VkDeviceMemory> globalShaderGameObjsMemory;

    GameObjectShaderData gameObjectShaderDataArray[NUM_OBJECTS];
    VkDeviceSize bufferSizeGamObjs;


    VkQueue _presentQueue = VK_NULL_HANDLE;
    VkCommandPool _commandPool;
    VkCommandPoolCreateInfo _commandPoolInfo{};
    std::vector<VkCommandBuffer> _commandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    VkDeviceCreateInfo _createInfo{};
    VkFenceCreateInfo fenceInfo{};
    VkSemaphoreCreateInfo semaphoreInfo{};
    VkSwapchainKHR _swapChain;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkPhysicalDeviceProperties properties{};
    std::vector<VkSemaphore> _imageAvailableSemaphores;
    std::vector<VkSemaphore> _renderFinishedSemaphores;
    std::vector<VkFence> _inFlightFences;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    // immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;


};



static void framebufferResizeCallback(SDL_Window* window, int width, int height)
{
    auto app = reinterpret_cast<QTDoughApplication*>(SDL_GetWindowData(window, "user_data"));
    app->framebufferResized = true;
};