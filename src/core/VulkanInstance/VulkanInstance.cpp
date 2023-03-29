#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/VulkanInstance/VulkanInstance.h>
#include <core/VulkanInstance/supportUtils.h>
#include <core/Logging/ErrorLogger.h>
#include <core/Logging/DebugMessenger.h>

#include <cstdint>
#include <iostream>


VulkanInstance::VulkanInstance()
{
    // TODO: put anything here?
}

VulkanInstance::VulkanInstance(const char *instanceApplicationName)
{
    if (supportUtils::m_enableValidationLayers && !supportUtils::checkValidationLayerSupport()) {
        throwDebugException("Validation layers requested but not availible.");
    }
    
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = instanceApplicationName;
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "No Engine";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;

    std::vector<const char *> extensions = supportUtils::getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (supportUtils::m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(supportUtils::m_validationLayers.size());
        createInfo.ppEnabledLayerNames = supportUtils::m_validationLayers.data();

        DebugMessenger::populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    VkResult instanceResult = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    if (instanceResult != VK_SUCCESS) {
        throwDebugException("Failed to create vulkan instance.");
    }

    DebugMessenger::createDebugMessenger(vkInstance, debugMessenger);
}
