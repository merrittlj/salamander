#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <core/Shader/Image.h>
#include <core/Shader/Depth.h>
#include <core/DisplayManager/DisplayManager.h>
#include <core/VulkanInstance/DeviceHandler.h>
#include <core/Command/CommandManager.h>
#include <core/Buffer/Buffer.h>
#include <core/Logging/ErrorLogger.h>

#include <string>
#include <vector>
#include <iostream>


void Image::ImageDetails::cleanupImageDetails(VkDevice vulkanLogicalDevice)
{
    vkDestroyImageView(vulkanLogicalDevice, this->imageView, nullptr);
    vkDestroyImage(vulkanLogicalDevice, this->image, nullptr);
    vkFreeMemory(vulkanLogicalDevice, this->imageMemory, nullptr);
}

void Image::TextureDetails::cleanupTextureDetails(VkDevice vulkanLogicalDevice)
{
    this->textureImageDetails.cleanupImageDetails(vulkanLogicalDevice);
    vkDestroySampler(vulkanLogicalDevice, this->textureSampler, nullptr);
}

void Image::populateImageDetails(uint32_t width, uint32_t height, uint32_t mipmapLevels, uint32_t layerCount, VkSampleCountFlagBits msaaSampleCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties, DeviceHandler::VulkanDevices vulkanDevices, Image::ImageDetails& imageDetails)
{
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;

    imageCreateInfo.mipLevels = mipmapLevels;
    imageCreateInfo.arrayLayers = layerCount;

    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // we generally transition the image to a transfer destination then copy.

    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    imageCreateInfo.samples = msaaSampleCount;

    imageCreateInfo.flags = (layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0);

    VkResult imageCreationResult = vkCreateImage(vulkanDevices.logicalDevice, &imageCreateInfo, nullptr, &imageDetails.image);
    if (imageCreationResult != VK_SUCCESS) {
        throwDebugException("Failed to create image.");
    }


    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(vulkanDevices.logicalDevice, imageDetails.image, &imageMemoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    memoryAllocateInfo.allocationSize = imageMemoryRequirements.size;
    uint32_t selectedMemoryTypeIndex;
    Buffer::locateMemoryType(vulkanDevices.physicalDevice, imageMemoryRequirements.memoryTypeBits, memoryProperties, selectedMemoryTypeIndex);
    memoryAllocateInfo.memoryTypeIndex = selectedMemoryTypeIndex;

    VkResult memoryAllocationResult = vkAllocateMemory(vulkanDevices.logicalDevice, &memoryAllocateInfo, nullptr, &imageDetails.imageMemory);
    if (memoryAllocationResult != VK_SUCCESS) {
        throwDebugException("Failed to allocate image memory.");
    }

    vkBindImageMemory(vulkanDevices.logicalDevice, imageDetails.image, imageDetails.imageMemory, 0);


    // image itself and image memory are populated in the struct previously in this function.
    imageDetails.imageLayout = imageCreateInfo.initialLayout;
    imageDetails.imageWidth = width;
    imageDetails.imageHeight = height;
    imageDetails.imageFormat = format;
}

void Image::populateTextureDetails(std::string textureImageFilePath, bool isCubemap, VkCommandPool commandPool, VkQueue commandQueue, DeviceHandler::VulkanDevices vulkanDevices, Image::TextureDetails& textureDetails)
{
    VkCommandBuffer disposableCommandBuffer;
    CommandManager::beginRecordingSingleSubmitCommands(commandPool, vulkanDevices.logicalDevice, disposableCommandBuffer);


    textureDetails.textureImageDetails.imageLayerCount = (isCubemap ? 6 : 1);
    

    std::vector<stbi_uc *> textureImagesPixels;
    textureImagesPixels.resize(textureDetails.textureImageDetails.imageLayerCount);
    for (size_t i = 0; i < textureDetails.textureImageDetails.imageLayerCount; i += 1) {
        std::string textureImageResolvedFilePath = (isCubemap ? (textureImageFilePath + std::to_string(static_cast<uint32_t>(i)) + ".png") : textureImageFilePath);
        textureImagesPixels[i] = stbi_load(textureImageResolvedFilePath.c_str(), &textureDetails.textureImageDetails.imageWidth, &textureDetails.textureImageDetails.imageHeight, &textureDetails.textureImageDetails.imageChannels, STBI_rgb_alpha);

        if (!textureImagesPixels[i]) {  // if image not loaded.
            throwDebugException("Failed to load texture image.");
        }
    }
    
    textureDetails.textureImageDetails.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;  // TODO: this format should be incorrect for data textures(normal maps, etc.), as they are generally created in linear space.

    VkDeviceSize textureImageSize = (textureDetails.textureImageDetails.imageWidth * textureDetails.textureImageDetails.imageHeight * 4);
    textureDetails.textureImageDetails.mipmapLevels = (isCubemap ? 1 : (static_cast<uint32_t>(std::floor(std::log2(std::max(textureDetails.textureImageDetails.imageWidth, textureDetails.textureImageDetails.imageHeight)))) + 1));  // get the correct amount of mipmap levels.
    

    // similar process to Buffer::createDataBufferComponents, but with an buffer and an image rather than a buffer and a buffer.
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    Buffer::createBufferComponents((textureImageSize * textureDetails.textureImageDetails.imageLayerCount), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vulkanDevices, stagingBuffer, stagingBufferMemory);

    // copy texture image pixel data into the staging buffer's memory.
    void *mappedStagingBufferMemory;
    vkMapMemory(vulkanDevices.logicalDevice, stagingBufferMemory, 0, textureImageSize, 0, &mappedStagingBufferMemory);

    for (size_t i = 0; i < textureDetails.textureImageDetails.imageLayerCount; i += 1) {
        memcpy((static_cast<char *>(mappedStagingBufferMemory) + (textureImageSize * i)), textureImagesPixels[i], textureImageSize);
    }    
    
    vkUnmapMemory(vulkanDevices.logicalDevice, stagingBufferMemory);

    for (stbi_uc *textureImagePixels : textureImagesPixels) {
        stbi_image_free(textureImagePixels);
    }

    int imageUsage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    Image::populateImageDetails(textureDetails.textureImageDetails.imageWidth, textureDetails.textureImageDetails.imageHeight, textureDetails.textureImageDetails.mipmapLevels, textureDetails.textureImageDetails.imageLayerCount, VK_SAMPLE_COUNT_1_BIT, textureDetails.textureImageDetails.imageFormat, VK_IMAGE_TILING_OPTIMAL, imageUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanDevices, textureDetails.textureImageDetails);

    Image::transitionImageLayout(textureDetails.textureImageDetails.image, textureDetails.textureImageDetails.imageFormat, textureDetails.textureImageDetails.mipmapLevels, textureDetails.textureImageDetails.imageLayerCount, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, disposableCommandBuffer);
    Image::copyBufferToImage(stagingBuffer, textureDetails.textureImageDetails.image, textureDetails.textureImageDetails.imageWidth, textureDetails.textureImageDetails.imageHeight, textureDetails.textureImageDetails.imageLayerCount, disposableCommandBuffer);

    if (isCubemap == true) {
        Image::transitionImageLayout(textureDetails.textureImageDetails.image, textureDetails.textureImageDetails.imageFormat, textureDetails.textureImageDetails.mipmapLevels, textureDetails.textureImageDetails.imageLayerCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, disposableCommandBuffer);
        textureDetails.textureImageDetails.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    
    CommandManager::submitSingleSubmitCommands(disposableCommandBuffer, commandPool, commandQueue, vulkanDevices.logicalDevice);

    vkDestroyBuffer(vulkanDevices.logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(vulkanDevices.logicalDevice, stagingBufferMemory, nullptr);


    Image::createImageView(textureDetails.textureImageDetails.image, VK_FORMAT_R8G8B8A8_SRGB, textureDetails.textureImageDetails.mipmapLevels, textureDetails.textureImageDetails.imageLayerCount, VK_IMAGE_ASPECT_COLOR_BIT, vulkanDevices.logicalDevice, textureDetails.textureImageDetails.imageView);

    if (isCubemap == false) {
        Image::generateMipmapLevels(textureDetails.textureImageDetails, commandPool, commandQueue, vulkanDevices);
        textureDetails.textureImageDetails.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    Image::createTextureSampler(vulkanDevices, textureDetails.textureImageDetails.mipmapLevels, textureDetails.textureSampler);
}

void Image::generateSwapchainImageDetails(DisplayManager::DisplayDetails& displayDetails, DeviceHandler::VulkanDevices vulkanDevices)
{
    Image::populateImageDetails(displayDetails.swapchainImageExtent.width, displayDetails.swapchainImageExtent.height, 1, 1, displayDetails.msaaSampleCount, displayDetails.swapchainImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanDevices, displayDetails.colorImageDetails);
    Image::createImageView(displayDetails.colorImageDetails.image, displayDetails.colorImageDetails.imageFormat, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, vulkanDevices.logicalDevice, displayDetails.colorImageDetails.imageView);

    Depth::populateDepthImageDetails(displayDetails.swapchainImageExtent, displayDetails.msaaSampleCount, 1, (VkImageUsageFlagBits)(0), displayDetails.graphicsCommandPool, displayDetails.graphicsQueue, vulkanDevices, displayDetails.depthImageDetails);
    Image::createImageView(displayDetails.depthImageDetails.image, displayDetails.depthImageDetails.imageFormat, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT, vulkanDevices.logicalDevice, displayDetails.depthImageDetails.imageView);
}

void Image::generateMipmapLevels(Image::ImageDetails& imageDetails, VkCommandPool commandPool, VkQueue commandQueue, DeviceHandler::VulkanDevices vulkanDevices)
{
    // check image format linear blitting support.
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(vulkanDevices.physicalDevice, imageDetails.imageFormat, &formatProperties);

    if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == false) {
        throwDebugException("Mipmap image format does not support linear blitting.");
    }
    
    
    VkCommandBuffer disposableCommandBuffer;
    CommandManager::beginRecordingSingleSubmitCommands(commandPool, vulkanDevices.logicalDevice, disposableCommandBuffer);


    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    
    imageMemoryBarrier.image = imageDetails.image;
    
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.subresourceRange.levelCount = 1;

    int32_t mipmapImageWidth = imageDetails.imageWidth;
    int32_t mipmapImageHeight = imageDetails.imageHeight;
    for (size_t i = 1; i < imageDetails.mipmapLevels; i += 1) {
        imageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;  // set the "source" of the mipmap level to the previous(blitted) mipmap level.

        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(disposableCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);


        VkImageBlit imageBlit{};

        // [0] and [1] as two mipmap levels are blitted at once.
        imageBlit.srcOffsets[0] = {0, 0, 0};
        imageBlit.dstOffsets[0] = {0, 0, 0};
        imageBlit.srcOffsets[1] = {mipmapImageWidth, mipmapImageHeight, 1};
        imageBlit.dstOffsets[1] = {(mipmapImageWidth > 1 ? mipmapImageWidth / 2 : 1), (mipmapImageHeight > 1 ? mipmapImageHeight / 2 : 1), 1};  // the second mipmap level is half the first, check if it is possible to keep dividing.

        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.dstSubresource.mipLevel = i;

        imageBlit.srcSubresource.baseArrayLayer = 0;
        imageBlit.dstSubresource.baseArrayLayer = 0;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(disposableCommandBuffer, imageDetails.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageDetails.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);  // must be submitted to a queue with graphics capability.


        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(disposableCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        

        if (mipmapImageWidth > 1) {
            mipmapImageWidth /= 2;
        }
        if (mipmapImageHeight > 1) {
            mipmapImageHeight /= 2;
        }
    }


    // transition the last image's layout.
    imageMemoryBarrier.subresourceRange.baseMipLevel = imageDetails.mipmapLevels - 1;
        
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(disposableCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);


    CommandManager::submitSingleSubmitCommands(disposableCommandBuffer, commandPool, commandQueue, vulkanDevices.logicalDevice);
}

void Image::populateImageViewCreateInfo(VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLevel, uint32_t layerCount, VkImageViewCreateInfo& imageViewCreateInfo)
{
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = format;

    imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
    imageViewCreateInfo.subresourceRange.baseMipLevel = baseMipLevel;
    imageViewCreateInfo.subresourceRange.levelCount = levelCount;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = baseArrayLevel;
    imageViewCreateInfo.subresourceRange.layerCount = layerCount;
}

void Image::createImageView(VkImage baseImage, VkFormat baseFormat, uint32_t mipmapLevels, uint32_t layerCount, VkImageAspectFlags imageAspectFlags, VkDevice vulkanLogicalDevice, VkImageView& imageView)
{
    VkImageViewCreateInfo imageViewCreateInfo{};
    Image::populateImageViewCreateInfo(baseImage, (layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D), baseFormat, imageAspectFlags, 0, mipmapLevels, 0, layerCount, imageViewCreateInfo);

    VkResult imageViewCreationResult = vkCreateImageView(vulkanLogicalDevice, &imageViewCreateInfo, nullptr, &imageView);
    if (imageViewCreationResult != VK_SUCCESS) {
        throwDebugException("Failed to create image view.");
    }
}

void Image::createTextureSampler(DeviceHandler::VulkanDevices vulkanDevices, uint32_t mipmapLevels, VkSampler& textureSampler)
{
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;

    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkPhysicalDeviceProperties physicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(vulkanDevices.physicalDevice, &physicalDeviceProperties);

    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;

    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;  // used when sampling out of the image in a clamp-to-border addressing mode.
    
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;  // we want to use coordinates in the range of 0..1.
    
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = static_cast<float>(mipmapLevels);

    VkResult textureSamplerCreationResult = vkCreateSampler(vulkanDevices.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
    if (textureSamplerCreationResult != VK_SUCCESS) {
        throwDebugException("Failed to create a texture sampler.");
    }
}

void Image::selectSupportedImageFormat(const std::vector<VkFormat> candidateImageFormats, VkImageTiling imageTiling, VkFormatFeatureFlags imageFormatFeatureFlags, VkPhysicalDevice vulkanPhysicalDevice, VkFormat& imageFormat)
{
    for (VkFormat candidateImageFormat : candidateImageFormats) {
        VkFormatProperties candidateImageFormatProperties;
        vkGetPhysicalDeviceFormatProperties(vulkanPhysicalDevice, candidateImageFormat, &candidateImageFormatProperties);

        // while we could use a ||(or) and use a single if statement, it would get too complex/unreadable.
        if ((imageTiling == VK_IMAGE_TILING_LINEAR) && ((candidateImageFormatProperties.linearTilingFeatures & imageFormatFeatureFlags) == imageFormatFeatureFlags)) {
            imageFormat = candidateImageFormat;
            return;
        } else if ((imageTiling == VK_IMAGE_TILING_OPTIMAL) && ((candidateImageFormatProperties.optimalTilingFeatures & imageFormatFeatureFlags) == imageFormatFeatureFlags)) {
            imageFormat = candidateImageFormat;
            return;
        }
    }

    throwDebugException("Failed to select a supported image format.");  // no supported image format was selected.
}

void Image::transitionImageLayout(VkImage image, VkFormat format, uint32_t mipmapLevels, uint32_t layerCount, VkImageLayout initialImageLayout, VkImageLayout targetImageLayout, VkCommandBuffer commandBuffer)
{
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    imageMemoryBarrier.oldLayout = initialImageLayout;
    imageMemoryBarrier.newLayout = targetImageLayout;

    // we aren't transferring explicit queue family ownership.
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.image = image;
    
    if (targetImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || targetImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {  // image is a depth/stencil attachment.
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        
        if (Depth::depthImageFormatHasStencilComponent(format)) {  // include stencil aspect mask if supported by the image format.
            imageMemoryBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {  // image is a color attachment.
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = mipmapLevels;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (initialImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && targetImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {  // transitioning to write image pixels.
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (initialImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && targetImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {  // transitioning to read from the fragment shader.
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (initialImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && targetImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {  // transitioning to use as a depth/stencil attachment.
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (initialImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && targetImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {  // transitioning to use as a depth/stencil attachment.
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        throwDebugException("Unsupported/invalid layout transition.");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void Image::copyBufferToImage(VkBuffer sourceBuffer, VkImage destinationImage, uint32_t imageWidth, uint32_t imageHeight, uint32_t imageLayerCount, VkCommandBuffer commandBuffer)
{
    VkBufferImageCopy bufferImageCopy{};

    bufferImageCopy.bufferOffset = 0;

    // image pixels are tightly packed.
    bufferImageCopy.bufferRowLength = 0;
    bufferImageCopy.bufferImageHeight = 0;

    bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopy.imageSubresource.mipLevel = 0;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;
    bufferImageCopy.imageSubresource.layerCount = imageLayerCount;

    bufferImageCopy.imageOffset = {0, 0, 0};
    bufferImageCopy.imageExtent = {imageWidth, imageHeight, 1};

    vkCmdCopyBufferToImage(commandBuffer, sourceBuffer, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
}
