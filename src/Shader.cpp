#include "Shader.h"

#include <fstream>

#include "Renderer.h"

namespace
{

std::vector<char> ReadFile(const std::string & filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("failed to open file!");

    size_t            fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

} // namespace

// ----------------------------------------------------------------------------
// Shader
// ----------------------------------------------------------------------------

Shader::Shader(const RenderContext & context)
    : m_context(context)
    , m_shaderModule(VK_NULL_HANDLE)
    , m_stageInfo {}
{
    // ...
}

Shader::Shader(Shader && other) noexcept
    : m_context(other.m_context)
    , m_stageInfo(other.m_stageInfo)
    , m_shaderModule(other.m_shaderModule)
{
    other.m_shaderModule = VK_NULL_HANDLE;
}

Shader & Shader::operator=(Shader && other) noexcept
{
    if (this != &other)
    {
        Cleanup();

        m_stageInfo    = other.m_stageInfo;
        m_shaderModule = other.m_shaderModule;

        other.m_shaderModule = VK_NULL_HANDLE;
    }

    return *this;
}

Shader::~Shader()
{
    Cleanup();
}

void Shader::LoadFromFile(const std::string & filepath, VkShaderStageFlagBits stage)
{
    auto shaderCode = ReadFile(filepath);

    m_shaderModule = CreateShaderModule(shaderCode);

    m_stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    m_stageInfo.stage  = stage;
    m_stageInfo.module = m_shaderModule;
    m_stageInfo.pName  = "main";
}

void Shader::Cleanup()
{
    if (m_shaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_context.device, m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
    }
}

VkShaderModule Shader::CreateShaderModule(const std::vector<char> & code)
{
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;

    if (vkCreateShaderModule(m_context.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");

    return shaderModule;
}
