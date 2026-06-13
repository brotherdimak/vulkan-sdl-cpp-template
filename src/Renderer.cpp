#include "Renderer.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Buffer.h"
#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"

#include "RenderPipeline.h"
#include "RenderUtils.h"
#include "SceneObject.h"
#include "UIRenderer.h"

namespace
{

const uint32_t MAX_FRAMES   = 2;
const uint32_t MAX_TEXTURES = 128;

const std::vector<const char *> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

} // namespace

// ----------------------------------------------------------------------------
// Renderer Static
// ----------------------------------------------------------------------------

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT              messageType,
    const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
    void *                                       pUserData
)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

// ----------------------------------------------------------------------------
// Renderer
// ----------------------------------------------------------------------------

Renderer::Renderer()
    : m_window(nullptr)
    , m_apiVersion(0)
    , m_instance(VK_NULL_HANDLE)
    , m_surface(VK_NULL_HANDLE)
    , m_debugMessenger(VK_NULL_HANDLE)
    , m_context {}
    , m_swapChain(VK_NULL_HANDLE)
    , m_swapChainImageFormat(VK_FORMAT_UNDEFINED)
    , m_swapChainExtent {}
    , m_minImageCount(0)
    , m_imageCount(0)
    , m_swapChainImages {}
    , m_swapChainImageViews {}
    , m_swapChainFramebuffers {}
    , m_depthImage(nullptr)
    , m_uniformBuffer(nullptr)
    , m_currentFrame(0)
    , m_framebufferResized(false)
    , m_commandBuffers {}
    , m_descriptorSets {}
    , m_imageAvailableSemaphores {}
    , m_renderFinishedSemaphores {}
    , m_inFlightFences {}
{
    // ...
}

Renderer::~Renderer()
{
    // ...
}

void Renderer::Init(SDL_Window * window)
{
    m_window            = window;
    m_context.maxFrames = MAX_FRAMES;

#ifdef NDEBUG
    m_context.isDebug = false;
#else
    m_context.isDebug = true;
#endif

    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateMemoryAllocator();

    CreateSwapChain();
    CreateImageViews();

    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();

    m_uniformBuffer = new UniformBuffer(m_context);

    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
}

void Renderer::CleanupSwapChain()
{
    delete m_depthImage;

    for (auto framebuffer : m_swapChainFramebuffers)
        vkDestroyFramebuffer(m_context.device, framebuffer, nullptr);

    for (auto imageView : m_swapChainImageViews)
        vkDestroyImageView(m_context.device, imageView, nullptr);

    vkDestroySwapchainKHR(m_context.device, m_swapChain, nullptr);
}

void Renderer::Cleanup()
{
    CleanupSwapChain();

    vkDestroyRenderPass(m_context.device, m_context.renderPass, nullptr);

    delete m_uniformBuffer;

    vkDestroyDescriptorPool(m_context.device, m_context.descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_context.device, m_context.globalDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_context.device, m_context.textureDescriptorSetLayout, nullptr);

    for (size_t i = 0; i < m_inFlightFences.size(); i++)
        vkDestroyFence(m_context.device, m_inFlightFences[i], nullptr);

    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++)
        vkDestroySemaphore(m_context.device, m_renderFinishedSemaphores[i], nullptr);

    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); i++)
        vkDestroySemaphore(m_context.device, m_imageAvailableSemaphores[i], nullptr);

    vkDestroyCommandPool(m_context.device, m_context.commandPool, nullptr);

    vmaDestroyAllocator(m_context.allocator);

    vkDestroyDevice(m_context.device, nullptr);

    if (m_context.isDebug)
        RenderUtils::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void Renderer::RecreateSwapChain()
{
    int width  = 0;
    int height = 0;

    SDL_GetWindowSize(m_window, &width, &height);

    while (width == 0 || height == 0)
    {
        SDL_GetWindowSize(m_window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    vkDeviceWaitIdle(m_context.device);

    CleanupSwapChain();

    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
}

void Renderer::CreateInstance()
{
    if (m_context.isDebug && !RenderUtils::CheckValidationLayerSupport(VALIDATION_LAYERS))
        throw std::runtime_error("validation layers requested, but not available!");

    VkApplicationInfo appInfo {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Vulkan Template";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo {};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions                    = RenderUtils::GetRequiredExtensions(m_context);
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};

    if (m_context.isDebug)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        RenderUtils::PopulateDebugMessengerCreateInfo(debugCreateInfo, Renderer::DebugCallback);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext             = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance!");

    vkEnumerateInstanceVersion(&m_apiVersion);
    SDL_Log("Vulkan version: %d.%d", VK_API_VERSION_MAJOR(m_apiVersion), VK_API_VERSION_MINOR(m_apiVersion));
}

void Renderer::SetupDebugMessenger()
{
    if (!m_context.isDebug)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    RenderUtils::PopulateDebugMessengerCreateInfo(createInfo, Renderer::DebugCallback);

    if (RenderUtils::CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to set up debug messenger!");
}

void Renderer::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface))
        throw std::runtime_error("failed to create window surface!");
}

void Renderer::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto & device : devices)
    {
        if (RenderUtils::IsDeviceSuitable(device, m_surface, DEVICE_EXTENSIONS))
        {
            m_context.physicalDevice = device;
            break;
        }
    }

    if (m_context.physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU!");

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_context.physicalDevice, &deviceProperties);

    SDL_Log("GPU Name: %s", deviceProperties.deviceName);
}

void Renderer::CreateLogicalDevice()
{
    QueueFamilyIndices indices = RenderUtils::FindQueueFamilies(m_context.physicalDevice, m_surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t>                   uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    // Enable dynamic rendering feature
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures {};
    dynamicRenderingFeatures.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    dynamicRenderingFeatures.pNext            = nullptr;

    // Chain the features together
    VkPhysicalDeviceFeatures2 deviceFeatures2 {};
    deviceFeatures2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext    = &dynamicRenderingFeatures;
    deviceFeatures2.features = deviceFeatures;

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos    = queueCreateInfos.data();

    createInfo.pNext = &deviceFeatures2;

    createInfo.enabledExtensionCount   = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

    createInfo.enabledLayerCount   = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    if (vkCreateDevice(m_context.physicalDevice, &createInfo, nullptr, &m_context.device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");

    vkGetDeviceQueue(m_context.device, indices.graphicsFamily.value(), 0, &m_context.graphicsQueue);
    vkGetDeviceQueue(m_context.device, indices.presentFamily.value(), 0, &m_context.presentQueue);

    m_context.graphicsQueueIndex = indices.graphicsFamily.value();
    m_context.presentQueueIndex  = indices.presentFamily.value();

    // Dynamic rendering (Vulkan >= 1.3)
    m_context.vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(m_context.device, "vkCmdBeginRenderingKHR");
    m_context.vkCmdEndRenderingKHR   = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(m_context.device, "vkCmdEndRenderingKHR");

    if (!m_context.vkCmdBeginRenderingKHR || !m_context.vkCmdEndRenderingKHR)
        throw std::runtime_error("Failed to load dynamic rendering function pointers!");
}

void Renderer::CreateMemoryAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice   = m_context.physicalDevice;
    allocatorInfo.device           = m_context.device;
    allocatorInfo.instance         = m_instance;

    if (vmaCreateAllocator(&allocatorInfo, &m_context.allocator) != VK_SUCCESS)
        throw std::runtime_error("failed to create memory allocator!");
}

void Renderer::CreateSwapChain()
{
    int windowWidth;
    int windowHeight;

    SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);

    SwapChainSupportDetails swapChainSupport = RenderUtils::QuerySwapChainSupport(m_context.physicalDevice, m_surface);

    VkSurfaceFormatKHR surfaceFormat = RenderUtils::ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR   presentMode   = RenderUtils::ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D         extent        = RenderUtils::ChooseSwapExtent(swapChainSupport.capabilities, windowWidth, windowHeight);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    m_minImageCount = swapChainSupport.capabilities.minImageCount;
    m_imageCount    = imageCount;

    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;

    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices              = RenderUtils::FindQueueFamilies(m_context.physicalDevice, m_surface);
    uint32_t           queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;

    if (vkCreateSwapchainKHR(m_context.device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain!");

    vkGetSwapchainImagesKHR(m_context.device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_context.device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent      = extent;
}

void Renderer::CreateImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
    {
        m_swapChainImageViews[i] = ImageUtils::CreateImageView(m_context, m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Renderer::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment {};
    colorAttachment.format         = m_swapChainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment {};
    depthAttachment.format         = RenderUtils::FindDepthFormat(m_context);
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo                 renderPassInfo {};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(m_context.device, &renderPassInfo, nullptr, &m_context.renderPass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");

    m_context.renderPass = m_context.renderPass;
}

void Renderer::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding {};
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo uboLayoutInfo {};
    uboLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    uboLayoutInfo.bindingCount = 1;
    uboLayoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_context.device, &uboLayoutInfo, nullptr, &m_context.globalDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create ubo descriptor set layout!");

    VkDescriptorSetLayoutBinding samplerLayoutBinding {};
    samplerLayoutBinding.binding            = 0;
    samplerLayoutBinding.descriptorCount    = 1;
    samplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo samplerLayoutInfo {};
    samplerLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    samplerLayoutInfo.bindingCount = 1;
    samplerLayoutInfo.pBindings    = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_context.device, &samplerLayoutInfo, nullptr, &m_context.textureDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create sampler descriptor set layout!");
}

void Renderer::CreateFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {m_swapChainImageViews[i], m_depthImage->GetView()};

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = m_context.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments    = attachments.data();
        framebufferInfo.width           = m_swapChainExtent.width;
        framebufferInfo.height          = m_swapChainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(m_context.device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }
}

void Renderer::CreateCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = RenderUtils::FindQueueFamilies(m_context.physicalDevice, m_surface);

    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_context.device, &poolInfo, nullptr, &m_context.commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics command pool!");

    m_context.commandPool = m_context.commandPool;
}

void Renderer::CreateDepthResources()
{
    m_depthImage = new Image(m_context);

    m_depthImage->Create(
        m_swapChainExtent.width,
        m_swapChainExtent.height,
        RenderUtils::FindDepthFormat(m_context),
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
}

void Renderer::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes {};

    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES;

    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_TEXTURES;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = MAX_FRAMES + MAX_TEXTURES;

    if (vkCreateDescriptorPool(m_context.device, &poolInfo, nullptr, &m_context.descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool!");
}

void Renderer::CreateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(m_context.maxFrames, m_context.globalDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_context.descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(m_context.maxFrames);
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSets.resize(m_context.maxFrames);

    if (vkAllocateDescriptorSets(m_context.device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets!");

    for (size_t i = 0; i < m_context.maxFrames; i++)
    {
        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = m_uniformBuffer->GetBuffer(i);
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite {};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = m_descriptorSets[i];
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(m_context.device, 1, &descriptorWrite, 0, nullptr);
    }
}

void Renderer::CreateCommandBuffers()
{
    m_commandBuffers.resize(m_context.maxFrames);

    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_context.commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

    if (vkAllocateCommandBuffers(m_context.device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");
}

void Renderer::CreateSyncObjects()
{
    m_inFlightFences.resize(m_context.maxFrames);

    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < m_inFlightFences.size(); i++)
    {
        if (vkCreateFence(m_context.device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    m_imageAvailableSemaphores.resize(m_context.maxFrames);
    m_renderFinishedSemaphores.resize(m_swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); i++)
    {
        if (vkCreateSemaphore(m_context.device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++)
    {
        if (vkCreateSemaphore(m_context.device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization objects for a frame!");
    }
}

// ----------------------------------------------------------------------------
// Render Loop
// ----------------------------------------------------------------------------

void Renderer::StartFrame()
{
    m_renderObjects.clear();
}

void Renderer::SubmitRenderObject(SceneObject * object)
{
    m_renderObjects.push_back(object);
}

void Renderer::DrawFrame(UIRenderer * uiRenderer, const Camera & camera, const RenderPipeline & pipeline)
{
    vkWaitForFences(m_context.device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result =
        vkAcquireNextImageKHR(m_context.device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swap chain image!");

    UpdateUniformBuffer(m_currentFrame, camera);

    vkResetFences(m_context.device, 1, &m_inFlightFences[m_currentFrame]);
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    // Draw Geometry
    RecordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex, pipeline);

    // Draw UI
    VkCommandBuffer uiCommandBuffer = uiRenderer->PrepareCommandBuffer(imageIndex);

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore          waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount         = 1;
    submitInfo.pWaitSemaphores            = waitSemaphores;
    submitInfo.pWaitDstStageMask          = waitStages;

    VkCommandBuffer commandBuffers[] = {m_commandBuffers[m_currentFrame], uiCommandBuffer};

    submitInfo.commandBufferCount = 2;
    submitInfo.pCommandBuffers    = commandBuffers;

    VkSemaphore signalSemaphores[]  = {m_renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(m_context.graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_swapChain};
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_context.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
    {
        m_framebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % m_context.maxFrames;
}

void Renderer::UpdateUniformBuffer(uint32_t currentImage, const Camera & camera)
{
    UniformBufferObject ubo {};

    ubo.view = camera.GetView();
    ubo.proj = camera.GetProj();

    ubo.proj[1][1] *= -1;

    m_uniformBuffer->Update(currentImage, ubo);
}

void Renderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const RenderPipeline & pipeline)
{
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer!");

    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = m_context.renderPass;
    renderPassInfo.framebuffer       = m_swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;

    std::array<VkClearValue, 2> clearValues {};
    clearValues[0].color        = {{0.44f, 0.40f, 0.48f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());

    VkViewport viewport {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)m_swapChainExtent.width;
    viewport.height   = (float)m_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Uniform binding
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline.GetPipelineLayout(),
        0,
        1,
        &m_descriptorSets[m_currentFrame],
        0,
        nullptr
    );

    // Render objects
    for (SceneObject * object : m_renderObjects)
        object->Draw(commandBuffer, pipeline.GetPipelineLayout());

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer!");
}