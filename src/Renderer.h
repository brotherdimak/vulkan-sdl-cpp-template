#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan_core.h>

struct VmaAllocator_T;
typedef struct VmaAllocator_T * VmaAllocator;

class UIRenderer;
class RenderPipeline;
class SceneObject;
class Image;
class UniformBuffer;
class Camera;

struct SDL_Window;

struct RenderContext
{
    uint32_t maxFrames;
    bool     isDebug;

    // Core
    VkDevice         device;
    VkPhysicalDevice physicalDevice;

    // Queues
    uint32_t graphicsQueueIndex;
    uint32_t presentQueueIndex;
    VkQueue  graphicsQueue;
    VkQueue  presentQueue;

    // Resources
    VmaAllocator     allocator;
    VkCommandPool    commandPool;
    VkDescriptorPool descriptorPool;

    // Layouts & Render Pass
    VkDescriptorSetLayout globalDescriptorSetLayout;
    VkDescriptorSetLayout textureDescriptorSetLayout;
    VkRenderPass          renderPass;

    // Dynamic Rendering (Vulkan >= 1.3) Now used for ImGui only!
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR   vkCmdEndRenderingKHR;
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
    void Init(SDL_Window * window);

    void StartFrame();
    void SubmitRenderObject(SceneObject * object);
    void DrawFrame(UIRenderer * uiRenderer, const Camera & camera, const RenderPipeline & pipeline);

    void Cleanup();

public:
    void OnWindowResize()
    {
        m_framebufferResized = true;
    }

public:
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
    void CreateSyncObjects();

    void UpdateUniformBuffer(uint32_t currentImage, const Camera & camera);
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const RenderPipeline & pipeline);

private:
    SDL_Window * m_window;

    uint32_t     m_apiVersion;
    VkInstance   m_instance;
    VkSurfaceKHR m_surface;

    VkDebugUtilsMessengerEXT m_debugMessenger;

    RenderContext m_context;

    VkSwapchainKHR             m_swapChain;
    VkFormat                   m_swapChainImageFormat;
    VkExtent2D                 m_swapChainExtent;
    uint32_t                   m_minImageCount;
    uint32_t                   m_imageCount;
    std::vector<VkImage>       m_swapChainImages;
    std::vector<VkImageView>   m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    Image *         m_depthImage;
    UniformBuffer * m_uniformBuffer;
    uint32_t        m_currentFrame;
    bool            m_framebufferResized;

    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkDescriptorSet> m_descriptorSets;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;

    std::vector<SceneObject *> m_renderObjects;
};