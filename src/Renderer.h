#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include <string>
#include <vector>

#include "Buffer.h"
#include "Camera.h"
#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"

class UIRenderer;
class RenderPipeline;
class SceneObject;

struct RenderContext
{
    uint32_t maxFrames;
    bool     isDebug;

    VkPhysicalDevice physicalDevice;
    VkDevice         device;
    VkQueue          graphicsQueue;

    VkCommandPool    commandPool;
    VkDescriptorPool descriptorPool;

    VkRenderPass renderPass;

    VkDescriptorSetLayout globalLayout;
    VkDescriptorSetLayout textureLayout;

    VmaAllocator allocator;
};

class Renderer
{
public:
    Renderer();

    Renderer(const Renderer & other)             = delete;
    Renderer & operator=(const Renderer & other) = delete;

    Renderer(Renderer && other)             = delete;
    Renderer & operator=(Renderer && other) = delete;

    ~Renderer();

public:
    void InitVulkan();

    void StartFrame();
    void SubmitRenderObject(SceneObject * object);

    void DrawFrame(UIRenderer * uiRenderer, const Camera & camera, const RenderPipeline & pipeline);

    void Cleanup();

public: // Event Handlers
    void OnWindowResize()
    {
        m_framebufferResized = true;
    }

public: // Dynamic Rendering (Vulkan >= 1.3)
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR = nullptr;
    PFN_vkCmdEndRenderingKHR   vkCmdEndRenderingKHR   = nullptr;

public: // Getters / Setters
    void SetWindow(SDL_Window * window)
    {
        m_window = window;
    }
    SDL_Window * GetWindow() const
    {
        return m_window;
    }

    const RenderContext & GetContext() const
    {
        return m_context;
    }

    uint32_t GetApiVersion() const
    {
        return m_apiVersion;
    }
    VkInstance GetInstance() const
    {
        return m_instance;
    }
    VkExtent2D GetFramebufferSize() const
    {
        return m_swapChainExtent;
    }
    VkPhysicalDevice GetPhysicalDevice() const
    {
        return m_physicalDevice;
    }
    VkDevice GetDevice() const
    {
        return m_device;
    }
    VkRenderPass GetRenderPass() const
    {
        return m_renderPass;
    }
    VkCommandPool GetCommandPool() const
    {
        return m_commandPool;
    }

    uint32_t GetPresentQueueFamilyIndex() const
    {
        return m_presentQueueIndex;
    }
    VkQueue GetPresentQueue() const
    {
        return m_presentQueue;
    }

    uint32_t GetMinImageCount() const
    {
        return m_minImageCount;
    }
    uint32_t GetImageCount() const
    {
        return m_imageCount;
    }

    VkFormat GetSwapChainImageFormat() const
    {
        return m_swapChainImageFormat;
    }

    VkImage GetSwapChainImage(uint32_t imageIndex) const
    {
        return m_swapChainImages.at(imageIndex);
    }
    VkImageView GetSwapChainImageView(uint32_t imageIndex) const
    {
        return m_swapChainImageViews.at(imageIndex);
    }

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT              messageType,
        const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
        void *                                       pUserData
    );

    void CleanupSwapChain();
    void RecreateSwapChain();
    void CreateInstance();

    void SetupDebugMessenger();

    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateMemoryAllocator();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateDepthResources();

    void CreateDescriptorPool();
    void CreateDescriptorSets();

    void CreateCommandBuffers();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const RenderPipeline & pipeline);
    void CreateSyncObjects();
    void UpdateUniformBuffer(uint32_t currentImage, const Camera & camera);

private:
    SDL_Window * m_window;

    RenderContext m_context;

    uint32_t                 m_apiVersion;
    VkInstance               m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR             m_surface;

    VkPhysicalDevice m_physicalDevice;
    VkDevice         m_device;

    uint32_t m_graphicsQueueIndex;
    uint32_t m_presentQueueIndex;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    VkSwapchainKHR             m_swapChain;
    std::vector<VkImage>       m_swapChainImages;
    VkFormat                   m_swapChainImageFormat;
    VkExtent2D                 m_swapChainExtent;
    std::vector<VkImageView>   m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    uint32_t                   m_minImageCount;
    uint32_t                   m_imageCount;

    VkRenderPass          m_renderPass;
    VkDescriptorSetLayout m_globalDescriptorSetLayout;
    VkDescriptorSetLayout m_textureDescriptorSetLayout;

    VkCommandPool m_commandPool;

    Image *         m_depthImage;
    UniformBuffer * m_uniformBuffer;

    VkDescriptorPool             m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;

    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;

    uint32_t m_currentFrame;
    bool     m_framebufferResized;

    std::vector<SceneObject *> m_renderObjects;
};