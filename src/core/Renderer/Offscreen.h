#ifndef OFFSCREEN_H
#define OFFSCREEN_H


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/Renderer/Pipeline.h>
#include <core/Shader/Image.h>
#include <core/VulkanInstance/DeviceHandler.h>

#include <vector>


namespace Offscreen
{
    struct OffscreenOperation {  // currently used for shadow mapping.
        VkExtent2D offscreenExtent;  // the extent of the framebuffer(differs from render extent when using a cubemap).
        VkExtent2D renderExtent;  // the extent of the area to render in.
        std::vector<VkFramebuffer> framebuffers;  // multiple frames-in-flight.

        Image::TextureDetails depthTextureDetails;  // framebuffer depth attachment.

        std::vector<VkImageView> imageViews;

        Pipeline::PipelineComponents pipelineComponents;
        VkRenderPass renderPass;

        bool beenGenerated = false;  // if the components have been generated before.


        // generate the member offscreen operation components.
        //
        // @param offscreenWidth the width of the offscreen "image".
        // @param offscreenHeight the height of the offscreen "image".
        // @param layerCount the amount of layers in the depth image.
        // @param createSpecializedRenderPass a passed in reference to a function used for creating the operation's render pass.
        // @param createSpecializedPipeline a passed in reference to a function used for creating the operation's pipeline.
        // @param graphicsCommandPool the command pool used for graphics operations.
        // @param graphicsQueue the queue used for graphics commands.
        // @param vulkanDevices Vulkan physical and logical device to use in member components generation.
        void generateMemberComponents(int32_t offscreenWidth, int32_t offscreenHeight, uint32_t layerCount, void (*createSpecializedRenderPass)(DeviceHandler::VulkanDevices, VkRenderPass&), void (*createSpecializedPipeline)(VkRenderPass, VkDevice, Pipeline::PipelineComponents&), VkCommandPool graphicsCommandPool, VkQueue graphicsQueue, DeviceHandler::VulkanDevices vulkanDevices);

        // cleanup the offscreen operation.
        //
        // @param vulkanLogicalDevice Vulkan logical device to use in offscreen operation cleanup.
        void cleanupOffscreenOperation(VkDevice vulkanLogicalDevice);
    };
}


#endif  // OFFSCREEN_H
