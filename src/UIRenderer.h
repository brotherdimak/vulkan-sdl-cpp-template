#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

class Renderer;

class UIRenderer
{
public:
    UIRenderer();

    UIRenderer(const UIRenderer & other)             = delete;
    UIRenderer & operator=(const UIRenderer & other) = delete;

    UIRenderer(UIRenderer && other)             = delete;
    UIRenderer & operator=(UIRenderer && other) = delete;

    ~UIRenderer();

    void Init(Renderer * renderer);
    void Cleanup();

    void BeginFrame();

    VkCommandBuffer PrepareCommandBuffer(int imageIndex);

private:
    void CreateDescriptorPool();
    void SetupImGui();

private:
    Renderer * m_Renderer;

    VkFormat         m_ColorFormat;
    VkFormat         m_DepthFormat;
    VkExtent2D       m_FrameBufferSize;
    VkDescriptorPool m_DescriptorPool;

    std::vector<VkCommandBuffer> m_CommandBuffers;
};