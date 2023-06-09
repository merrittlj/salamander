#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <core/Shader/Depth.h>
#include <core/Shader/Image.h>
#include <core/Renderer/Renderer.h>
#include <core/VulkanInstance/DeviceHandler.h>
#include <core/Command/CommandManager.h>
#include <core/Logging/ErrorLogger.h>


void Depth::populateDepthImageDetails(VkExtent2D swapchainImageExtent, VkSampleCountFlagBits msaaSampleCount, uint32_t layerCount, VkImageUsageFlagBits additionalImageUsage, VkCommandPool commandPool, VkQueue commandQueue, DeviceHandler::VulkanDevices vulkanDevices, Image::ImageDetails& depthImageDetails)
{
    Depth::selectDepthImageFormat(vulkanDevices.physicalDevice, depthImageDetails.imageFormat);
    Image::populateImageDetails(swapchainImageExtent.width, swapchainImageExtent.height, 1, layerCount, msaaSampleCount, depthImageDetails.imageFormat, VK_IMAGE_TILING_OPTIMAL, (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalImageUsage), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanDevices, depthImageDetails);

    
    VkCommandBuffer disposableCommandBuffer;
    CommandManager::beginRecordingSingleSubmitCommands(commandPool, vulkanDevices.logicalDevice, disposableCommandBuffer);

    VkImageLayout targetImageLayout = (additionalImageUsage == VK_IMAGE_USAGE_SAMPLED_BIT ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    Image::transitionImageLayout(depthImageDetails.image, depthImageDetails.imageFormat, 1, layerCount, VK_IMAGE_LAYOUT_UNDEFINED, targetImageLayout, disposableCommandBuffer);
    depthImageDetails.imageLayout = targetImageLayout;
    
    CommandManager::submitSingleSubmitCommands(disposableCommandBuffer, commandPool, commandQueue, vulkanDevices.logicalDevice);
}

void Depth::selectDepthImageFormat(VkPhysicalDevice vulkanPhysicalDevice, VkFormat& depthImageFormat)
{
    const std::vector<VkFormat> candidateDepthImageFormats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    Image::selectSupportedImageFormat(candidateDepthImageFormats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, vulkanPhysicalDevice, depthImageFormat);
}

bool Depth::depthImageFormatHasStencilComponent(VkFormat depthImageFormat)
{
    return depthImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthImageFormat == VK_FORMAT_D24_UNORM_S8_UINT;
}
