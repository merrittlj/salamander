#ifndef VULKANINSTANCE_H
#define VULKANINSTANCE_H


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/DisplayManager/DisplayManager.h>
#include <core/VulkanInstance/DeviceHandler.h>
#include <core/Queue/Queue.h>

#include <string>


class VulkanInstance
{
private:    
    // create a Vulkan instance using the supplied parameters.
    //
    // @param instanceApplicationName application name to use in the Vulkan instance.
    // @param createdInstance stored created Vulkan instance.
    void createVkInstance(std::string instanceApplicationName, VkInstance& createdInstance);
public:
    VkInstance m_vkInstance;  // actual Vulkan instance.
    
    VkDebugUtilsMessengerEXT m_debugMessenger;  // debug/logging messenger.
    
    Queue::QueueFamilyIndices m_familyIndices;

    DeviceHandler::VulkanDevices m_devices;  // selected physical device and created logical device.


    // clean up this class/instance.
    //
    // @param displayDetails display details.to use in clean up.
    void cleanupInstance(DisplayManager::DisplayDetails displayDetails);

    VulkanInstance();
    VulkanInstance(std::string instanceApplicationName, DisplayManager::DisplayDetails& displayDetails);
};


#endif  // VULKAN_INSTANCE_H
