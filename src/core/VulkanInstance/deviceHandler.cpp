#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/VulkanInstance/deviceHandler.h>
#include <core/VulkanInstance/VulkanInstance.h>
#include <core/VulkanInstance/supportUtils.h>
#include <core/DisplayManager/swapchainHandler.h>
#include <core/Queue/Queue.h>
#include <core/Logging/ErrorLogger.h>

#include <vector>
#include <set>


void deviceHandler::selectPhysicalDevice(VkInstance vkInstance, VkSurfaceKHR windowSurface, Queue::QueueFamilyIndices& queueFamilyIndices, VkPhysicalDevice& selectedPhysicalDevice)
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0) {
        throwDebugException("failed to find any GPUs with Vulkan support.");
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices.data());
    for (VkPhysicalDevice physicalDevice : physicalDevices) {
        if (deviceHandler::deviceSuitable(physicalDevice, windowSurface, queueFamilyIndices)) {
            selectedPhysicalDevice = physicalDevice;
            break;
        }
    }

    if (selectedPhysicalDevice == VK_NULL_HANDLE) {  // resultPhysicalDevice initialized to VK_NULL_HANDLE.
        throwDebugException("failed to find a suitable GPU.");
    }
}

bool deviceHandler::deviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR windowSurface, Queue::QueueFamilyIndices& queueFamilyIndices)
{
    // TODO: ranking system depending on necessary features, if the device is a dedicated graphics card, etc.
    bool extensionsSupported = deviceHandler::deviceExtensionsSuitable(physicalDevice);

    bool swapchainDetailsComplete = false;
    if (extensionsSupported) {
        swapchainHandler::SwapchainSupportDetails swapchainSupportDetails;
        swapchainHandler::querySwapchainSupportDetails(physicalDevice, windowSurface, swapchainSupportDetails);
        swapchainDetailsComplete = !swapchainSupportDetails.supportedSurfaceFormats.empty() && !swapchainSupportDetails.supportedPresentationModes.empty();
    }
    
    bool queueFamiliesSupported = Queue::deviceQueueFamiliesSuitable(physicalDevice, windowSurface, queueFamilyIndices);
    
    return extensionsSupported && swapchainDetailsComplete && queueFamiliesSupported;
}

bool deviceHandler::deviceExtensionsSuitable(VkPhysicalDevice physicalDevice)
{
    uint32_t supportedDeviceExtensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedDeviceExtensionCount, nullptr);

    std::vector<VkExtensionProperties> supportedDeviceExtensions(supportedDeviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedDeviceExtensionCount, supportedDeviceExtensions.data());

    std::set<std::string> requiredExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
    for (VkExtensionProperties extensionProperties : supportedDeviceExtensions) {
        requiredExtensions.erase(extensionProperties.extensionName);  // remove the required extension from the required extensions copy if matching.
    }

    return requiredExtensions.empty();  // if all the required extensions were found(removed individually from required extensions list.
}

void deviceHandler::createLogicalDevice(VkPhysicalDevice physicalDevice, Queue::QueueFamilyIndices queueFamilyIndices, VkDevice& createdLogicalDevice)
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    Queue::populateQueueCreateInfos(queueFamilyIndices, queueCreateInfos);
    
    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo logicalCreateInfo{};
    logicalCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicalCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    logicalCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    logicalCreateInfo.pEnabledFeatures = &deviceFeatures;
    logicalCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
    logicalCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    if (supportUtils::enableValidationLayers) {
        logicalCreateInfo.enabledLayerCount = static_cast<uint32_t>(supportUtils::requiredValidationLayers.size());
        logicalCreateInfo.ppEnabledLayerNames = supportUtils::requiredValidationLayers.data();
    } else {
        logicalCreateInfo.enabledLayerCount = 0;
    }

    int creationResult = vkCreateDevice(physicalDevice, &logicalCreateInfo, nullptr, &createdLogicalDevice);
    if (creationResult != VK_SUCCESS) {
        throwDebugException("Failed to create logical device.");
    }
}
