#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

struct RenderContext;

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class RenderUtils
{
public: // Command Buffers
    static VkCommandBuffer BeginSingleTimeCommands(const RenderContext & context);

    static void EndSingleTimeCommands(const RenderContext & context, VkCommandBuffer commandBuffer);

    static void AddImageMemoryBarrier(
        VkCommandBuffer commandBuffer,
        VkImage         image,
        VkImageLayout   oldLayout,
        VkImageLayout   newLayout
    );

public: // Formats & Capabilites
    static uint32_t
    FindMemoryType(const RenderContext & context, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    static VkFormat FindSupportedFormat(
        const RenderContext &         context,
        const std::vector<VkFormat> & candidates,
        VkImageTiling                 tiling,
        VkFormatFeatureFlags          features
    );

    static VkFormat FindDepthFormat(const RenderContext & context);

    static bool HasStencilComponent(VkFormat format);

    static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> & availableFormats);

    static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> & availablePresentModes);

    static VkExtent2D
    ChooseSwapExtent(const VkSurfaceCapabilitiesKHR & capabilities, int windowWidth, int windowHeight);

    static bool
    IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char *> extensions);

    static bool
    CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char *> & deviceExtensions);

    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    static std::vector<const char *> GetRequiredExtensions(const RenderContext & context);

    static bool CheckValidationLayerSupport(const std::vector<const char *> & validationLayers);

public: // Debug
    static VkResult CreateDebugUtilsMessengerEXT(
        VkInstance                                 instance,
        const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
        const VkAllocationCallbacks *              pAllocator,
        VkDebugUtilsMessengerEXT *                 pDebugMessenger
    );

    static void DestroyDebugUtilsMessengerEXT(
        VkInstance                    instance,
        VkDebugUtilsMessengerEXT      debugMessenger,
        const VkAllocationCallbacks * pAllocator
    );

    static void PopulateDebugMessengerCreateInfo(
        VkDebugUtilsMessengerCreateInfoEXT & createInfo,
        PFN_vkDebugUtilsMessengerCallbackEXT callback
    );

    static void ShowMemoryUsage(const RenderContext & context, bool isDetailed);
};