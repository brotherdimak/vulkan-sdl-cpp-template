#include "RenderUtils.h"
#include "Renderer.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>

// ----------------------------------------------------------------------------
// Command Buffers
// ----------------------------------------------------------------------------

VkCommandBuffer RenderUtils::BeginSingleTimeCommands(const RenderContext & context)
{
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = context.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void RenderUtils::EndSingleTimeCommands(const RenderContext & context, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.graphicsQueue);

    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
}

void RenderUtils::AddImageMemoryBarrier(
    VkCommandBuffer commandBuffer,
    VkImage         image,
    VkImageLayout   oldLayout,
    VkImageLayout   newLayout
)
{
    VkImageMemoryBarrier barrier {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;

        sourceStage      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ----------------------------------------------------------------------------
// Formats & Capabilites
// ----------------------------------------------------------------------------

uint32_t
RenderUtils::FindMemoryType(const RenderContext & context, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

VkFormat RenderUtils::FindSupportedFormat(
    const RenderContext &         context,
    const std::vector<VkFormat> & candidates,
    VkImageTiling                 tiling,
    VkFormatFeatureFlags          features
)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(context.physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat RenderUtils::FindDepthFormat(const RenderContext & context)
{
    return FindSupportedFormat(
        context,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool RenderUtils::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

SwapChainSupportDetails RenderUtils::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR RenderUtils::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> & availableFormats)
{
    for (const auto & availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR RenderUtils::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> & availablePresentModes)
{
    for (const auto & availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return availablePresentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
RenderUtils::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR & capabilities, int windowWidth, int windowHeight)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D actualExtent = {static_cast<uint32_t>(windowWidth), static_cast<uint32_t>(windowHeight)};

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

bool RenderUtils::IsDeviceSuitable(
    VkPhysicalDevice                device,
    VkSurfaceKHR                    surface,
    const std::vector<const char *> extensions
)
{
    QueueFamilyIndices indices = RenderUtils::FindQueueFamilies(device, surface);

    bool extensionsSupported = RenderUtils::CheckDeviceExtensionSupport(device, extensions);
    bool swapChainAdequate   = false;

    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = RenderUtils::QuerySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool RenderUtils::CheckDeviceExtensionSupport(
    VkPhysicalDevice                  device,
    const std::vector<const char *> & deviceExtensions
)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto & extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();
}

QueueFamilyIndices RenderUtils::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;

    for (const auto & queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport)
            indices.presentFamily = i;

        if (indices.isComplete())
            break;

        i++;
    }

    return indices;
}

std::vector<const char *> RenderUtils::GetRequiredExtensions(const RenderContext & context)
{
    uint32_t sdlExtensionCount = 0;
    auto     sdlExtensions     = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

    std::vector<const char *> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

    if (context.isDebug)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

bool RenderUtils::CheckValidationLayerSupport(const std::vector<const char *> & validationLayers)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char * layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto & layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
            return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------

VkResult RenderUtils::CreateDebugUtilsMessengerEXT(
    VkInstance                                 instance,
    const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
    const VkAllocationCallbacks *              pAllocator,
    VkDebugUtilsMessengerEXT *                 pDebugMessenger
)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void RenderUtils::DestroyDebugUtilsMessengerEXT(
    VkInstance                    instance,
    VkDebugUtilsMessengerEXT      debugMessenger,
    const VkAllocationCallbacks * pAllocator
)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr)
        func(instance, debugMessenger, pAllocator);
}

void RenderUtils::PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT & createInfo,
    PFN_vkDebugUtilsMessengerCallbackEXT callback
)
{
    createInfo       = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = callback;
}

void RenderUtils::ShowMemoryUsage(const RenderContext & context, bool isDetailed)
{
    VmaTotalStatistics stats;
    vmaCalculateStatistics(context.allocator, &stats);

    if (isDetailed)
    {
        char * statsString = nullptr;
        vmaBuildStatsString(context.allocator, &statsString, VK_TRUE);
        SDL_Log("%s\n", statsString);
        vmaFreeStatsString(context.allocator, statsString);
    }
    else
    {
        float totalAllocatedMB = stats.total.statistics.allocationBytes / (1024.0f * 1024.0f);
        float totalUsedMB      = stats.total.statistics.blockBytes / (1024.0f * 1024.0f);

        SDL_Log("================== Vulkan Memory Stats ==================");
        SDL_Log("Total Allocated: %.2f MB", totalAllocatedMB);
        SDL_Log("Total Used: %.2f MB", totalUsedMB);
        SDL_Log("Allocations: %u", stats.total.statistics.allocationCount);
        SDL_Log("Blocks: %u", stats.total.statistics.blockCount);
        SDL_Log("=========================================================");

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        {
            if (stats.memoryType[i].statistics.blockBytes > 0)
            {
                float typeMB      = stats.memoryType[i].statistics.blockBytes / (1024.0f * 1024.0f);
                float typeAllocMB = stats.memoryType[i].statistics.allocationBytes / (1024.0f * 1024.0f);

                uint32_t heapIndex  = memProps.memoryTypes[i].heapIndex;
                uint32_t blockCount = stats.memoryType[i].statistics.blockCount;

                VkMemoryPropertyFlags flags = memProps.memoryTypes[i].propertyFlags;

                std::string properties;
                if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    properties += "DEVICE_LOCAL ";
                if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    properties += "HOST_VISIBLE ";
                if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                    properties += "HOST_COHERENT ";
                if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                    properties += "HOST_CACHED ";
                if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
                    properties += "LAZILY_ALLOCATED ";

                SDL_Log(
                    "Type %u (%u): %.2f MB (%.2f MB used) - Blocks: %u",
                    i,
                    heapIndex,
                    typeMB,
                    typeAllocMB,
                    blockCount
                );
                SDL_Log("Properties: %s", properties.empty() ? "NONE" : properties.c_str());
                SDL_Log("=========================================================");
            }
        }
    }
}