#include "RayTracerPass.h"
#include "VoxelizerPass.h"
#include <random>

RayTracerPass::~RayTracerPass() {
    PassName = "RayTracerPass";
}

RayTracerPass::RayTracerPass() {
    PassName = "RayTracerPass";
    PassNames.push_back("RayTracerPass");


    QTDoughApplication* app = QTDoughApplication::instance;
    passWidth = app->swapChainExtent.width;
    passHeight = app->swapChainExtent.height;
}

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

void RayTracerPass::CreateMaterials() {

}

void RayTracerPass::CreateComputePipeline()
{
    QTDoughApplication* app = QTDoughApplication::instance;

    //Load RT function pointers
    vkCreateRayTracingPipelinesKHR_fn =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkCreateRayTracingPipelinesKHR");
    vkGetRayTracingShaderGroupHandlesKHR_fn =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");
    vkCmdTraceRaysKHR_fn =
        (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(app->_logicalDevice, "vkCmdTraceRaysKHR");

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

}

void RayTracerPass::CreateComputeDescriptorSets()
{

}

void RayTracerPass::UpdateUniformBuffer(VkCommandBuffer commandBuffer, uint32_t currentImage, uint32_t currentFrame, UnigmaCameraStruct& CameraMain) {

}


void RayTracerPass::CreateComputeDescriptorSetLayout()
{

}

void RayTracerPass::CreateShaderStorageBuffers()
{

}


void RayTracerPass::Dispatch(VkCommandBuffer commandBuffer, uint32_t currentFrame) {

}

void RayTracerPass::DebugCompute(uint32_t currentFrame)
{

}