#include "UIRenderer.h"

#include <stdexcept>

#include "Renderer.h"
#include "RenderUtils.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

#ifndef IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE
#define IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE 500
#endif

UIRenderer::UIRenderer()
    : m_Renderer(nullptr)
    , m_ColorFormat(VK_FORMAT_UNDEFINED)
    , m_DepthFormat(VK_FORMAT_UNDEFINED)
    , m_FrameBufferSize {0, 0}
    , m_DescriptorPool(VK_NULL_HANDLE)
{
    // ...
}

UIRenderer::~UIRenderer()
{
    // ...
}

void UIRenderer::Init(Renderer * renderer)
{
    m_Renderer        = renderer;
    m_FrameBufferSize = m_Renderer->GetFramebufferSize();

    CreateDescriptorPool();
    SetupImGui();
}

void UIRenderer::Cleanup()
{
    auto & context = m_Renderer->GetContext();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_DescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(context.device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    m_CommandBuffers.clear();
}

void UIRenderer::BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

VkCommandBuffer UIRenderer::PrepareCommandBuffer(int imageIndex)
{
    auto & context = m_Renderer->GetContext();

    // Update display size
    m_FrameBufferSize = m_Renderer->GetFramebufferSize();

    ImGuiIO & io     = ImGui::GetIO();
    io.DisplaySize.x = static_cast<float>(m_FrameBufferSize.width);
    io.DisplaySize.y = static_cast<float>(m_FrameBufferSize.height);

    ImGui::Render();

    // Record commands
    VkCommandBuffer commandBuffer = m_CommandBuffers[imageIndex];
    VkImage         image         = m_Renderer->GetSwapChainImage(imageIndex);
    VkImageView     imageView     = m_Renderer->GetSwapChainImageView(imageIndex);

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    RenderUtils::AddImageMemoryBarrier(commandBuffer, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfoKHR colorAttachment {};
    colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView   = imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfoKHR renderingInfo {};
    renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea.offset    = {0, 0};
    renderingInfo.renderArea.extent    = m_FrameBufferSize;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;

    // Dynamic Rendering (Vulkan >= 1.3)
    context.vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    context.vkCmdEndRenderingKHR(commandBuffer);

    RenderUtils::AddImageMemoryBarrier(commandBuffer, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(commandBuffer);

    return commandBuffer;
}

void UIRenderer::CreateDescriptorPool()
{
    auto & context = m_Renderer->GetContext();

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE}
    };

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 0;

    for (VkDescriptorPoolSize & poolSize : poolSizes)
        poolInfo.maxSets += poolSize.descriptorCount;

    poolInfo.poolSizeCount = (uint32_t)IM_COUNTOF(poolSizes);
    poolInfo.pPoolSizes    = poolSizes;

    vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &m_DescriptorPool);

    m_CommandBuffers.resize(m_Renderer->GetImageCount());

    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = context.commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

    if (vkAllocateCommandBuffers(context.device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate imgui command buffers!");
}

void UIRenderer::SetupImGui()
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    io.DisplaySize.x = static_cast<float>(m_FrameBufferSize.width);
    io.DisplaySize.y = static_cast<float>(m_FrameBufferSize.height);

    ImGui::GetStyle().FontScaleMain = 1.0f;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform / Renderer backends
    ImGui_ImplSDL3_InitForVulkan(m_Renderer->GetWindow());

    auto & context = m_Renderer->GetContext();

    ImGui_ImplVulkan_InitInfo initInfo {};
    initInfo.ApiVersion     = m_Renderer->GetApiVersion();
    initInfo.Instance       = m_Renderer->GetInstance();
    initInfo.PhysicalDevice = context.physicalDevice;
    initInfo.Device         = context.device;
    initInfo.QueueFamily    = context.presentQueueIndex;
    initInfo.Queue          = context.presentQueue;
    initInfo.PipelineCache  = nullptr;
    initInfo.DescriptorPool = m_DescriptorPool;
    initInfo.MinImageCount  = m_Renderer->GetMinImageCount();
    initInfo.ImageCount     = m_Renderer->GetImageCount();
    initInfo.Allocator      = nullptr;

    // Store formats as member variables since we need them to persist
    m_ColorFormat = m_Renderer->GetSwapChainImageFormat();
    m_DepthFormat = RenderUtils::FindDepthFormat(context);

    // Dynamic rendering (no separate render pass)
    initInfo.UseDynamicRendering = true;

    initInfo.PipelineInfoMain.RenderPass  = nullptr;
    initInfo.PipelineInfoMain.Subpass     = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_ColorFormat;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat   = m_DepthFormat;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    ImGui_ImplVulkan_Init(&initInfo);
}