#pragma once

#include <string>
#include <vulkan/vulkan.h>

struct RenderContext;

class RenderPipeline
{
public:
    RenderPipeline(
        const RenderContext & context,
        const std::string &   vertShaderPath,
        const std::string &   fragShaderPath
    );

    RenderPipeline(RenderPipeline & other)             = delete;
    RenderPipeline & operator=(RenderPipeline & other) = delete;

    RenderPipeline(RenderPipeline && other) noexcept;
    RenderPipeline & operator=(RenderPipeline && other) noexcept;

    ~RenderPipeline();

public:
    VkPipelineLayout GetPipelineLayout() const
    {
        return m_pipelineLayout;
    }

    VkPipeline GetPipeline() const
    {
        return m_graphicsPipeline;
    }

private:
    void CreateGraphicsPipeline(const std::string & vertShaderPath, const std::string & fragShaderPath);

    void Cleanup();

private:
    const RenderContext & m_context;

    VkPipelineLayout m_pipelineLayout;
    VkPipeline       m_graphicsPipeline;
};