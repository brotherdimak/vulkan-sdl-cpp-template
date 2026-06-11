#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct RenderContext;

class Shader
{
public:
    Shader(const RenderContext & context);

    Shader(const Shader &)             = delete;
    Shader & operator=(const Shader &) = delete;

    Shader(Shader && other) noexcept;
    Shader & operator=(Shader && other) noexcept;

    ~Shader();

public:
    void LoadFromFile(const std::string & filepath, VkShaderStageFlagBits stage);
    void Cleanup();

    VkPipelineShaderStageCreateInfo GetStageInfo() const
    {
        return m_stageInfo;
    }

private:
    VkShaderModule CreateShaderModule(const std::vector<char> & code);

private:
    const RenderContext & m_context;

    VkShaderModule                  m_shaderModule;
    VkPipelineShaderStageCreateInfo m_stageInfo;
};