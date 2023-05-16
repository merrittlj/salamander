#ifndef UNIFORM_H
#define UNIFORM_H


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <core/VulkanInstance/DeviceHandler.h>

#include <vector>


namespace Uniform
{
    struct UniformBufferObject {  // identical to the vertex shader's struct.
        alignas(16) glm::mat4 modelMatrix;
        alignas(16) glm::mat4 viewMatrix;
        alignas(16) glm::mat4 projectionMatrix;
    };


    // create multiple uniform buffers(one for each frame in flight).
    //
    // @param vulkanDevices Vulkan physical and logical device.
    // @param uniformBuffers created uniform buffers.
    // @param uniformBuffersMemory allocated uniform buffers memory.
    // @param mappedUniformBuffersMemory mapped uniform buffers memory.
    void createUniformBuffers(DeviceHandler::VulkanDevices vulkanDevices, std::vector<VkBuffer>& uniformBuffers, std::vector<VkDeviceMemory>& uniformBuffersMemory, std::vector<void *>& mappedUniformBuffersMemory);

    // update the passed frame's vertex shader uniform buffer.
    //
    // @param currentImage current image/frame.
    // @param swapchainImageExtent Vulkan swapchain image extent.
    // @param mappedUniformBuffersMemory mapped uniform buffers memory.
    void updateFrameUniformBuffer(size_t currentImage, VkExtent2D swapchainImageExtent, std::vector<void *>& mappedUniformBuffersMemory);
}


#endif  // UNIFORM_H