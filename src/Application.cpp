#include "Application.h"

#include <chrono>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#include "SceneObject.h"
#include "RenderPipeline.h"
#include "RenderUtils.h"

Application::Application()
    : m_isRunning(true)
    , m_isMinimized(false)
    , m_deltaTime(0.0f)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        throw std::runtime_error("Failed to initialize SDL");

    if (!SDL_Vulkan_LoadLibrary(nullptr))
        throw std::runtime_error("Failed to load Vulkan library");

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

    m_window = SDL_CreateWindow("Vulkan Template", 1280, 720, flags);

    if (!m_window)
        throw std::runtime_error("Failed to create window");
}

Application::~Application()
{
    // Wait for GPU to finish all pending operations
    vkDeviceWaitIdle(m_renderer.GetContext().device);

    // Destroy Vulkan resources
    m_sceneObjects.clear();
    m_pipelines.clear();

    // Clean up renderer
    m_uiRenderer.Cleanup();
    m_renderer.Cleanup();

    SDL_DestroyWindow(m_window);
}

void Application::Init()
{
    m_renderer.Init(m_window);
    m_uiRenderer.Init(&m_renderer);

    InitCamera();
    InitPipelines();
    InitSceneObjects();
}

void Application::Run()
{
    SDL_ShowWindow(m_window);

    while (m_isRunning)
    {
        HandleEvents();
        CalcDeltaTime();

        if (!m_isMinimized)
        {
            m_camera.HandleInput(m_window, m_deltaTime);

            m_uiRenderer.BeginFrame();
            BuildApplicationUI();

            m_renderer.StartFrame();

            for (auto & sceneObject : m_sceneObjects)
            {
                if (sceneObject.GetVisible())
                    m_renderer.SubmitRenderObject(&sceneObject);
            }

            m_renderer.DrawFrame(&m_uiRenderer, m_camera, m_pipelines.at(m_activePipeline));
        }
    }
}

void Application::InitCamera()
{
    int screenWidth, screenHeight;
    SDL_GetWindowSize(m_window, &screenWidth, &screenHeight);

    m_camera.SetScreenSize(screenWidth, screenHeight);
    m_camera.SetFov(70.0f);
    m_camera.SetPosition(glm::vec3(1.0f, -13.0f, 16.5f));
    m_camera.SetPitch(-13.0f);
    m_camera.SetYaw(138.0f);
}

void Application::InitPipelines()
{
    auto & context = m_renderer.GetContext();

    m_pipelines.try_emplace("Basic", context, "res/shaders/basic_vert.spv", "res/shaders/basic_frag.spv");
    m_pipelines.try_emplace("UV Gradient", context, "res/shaders/basic_vert.spv", "res/shaders/gradient_frag.spv");

    m_activePipeline = "Basic";
}

void Application::InitSceneObjects()
{
    auto & context = m_renderer.GetContext();

    m_sceneObjects.emplace_back(
        context,
        "Rock", "res/models/rock.png", "res/models/rock.obj",
        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 90.0f), 1.0f
    );

    m_sceneObjects.emplace_back(
        context,
        "Lighthouse", "res/models/lighthouse.png", "res/models/lighthouse.obj",
        glm::vec3(-12.32f, 4.51f, 8.22f), glm::vec3(0.0f, 0.0f, 90.0f), 1.0f
    );

    m_sceneObjects.emplace_back(
        context,
        "Tree First", "res/models/tree.png", "res/models/tree.obj",
        glm::vec3(1.0f, -3.61f, 8.0f), glm::vec3(0.0f, 0.0f, 0.0f), 1.0f
    );

    m_sceneObjects.emplace_back(
        context,
        "Tree Second", "res/models/tree.png", "res/models/tree.obj",
        glm::vec3(-5.45f, 3.65f, 8.24f), glm::vec3(0.0f, 0.0f, 60.0f), 1.0f
    );

    m_sceneObjects.emplace_back(
        context,
        "Tree Third", "res/models/tree.png", "res/models/tree.obj",
        glm::vec3(-12.26f, -5.76f, 6.06f), glm::vec3(0.0f, 0.0f, 120.0f), 1.2f
    );
}

void Application::HandleEvents()
{
    for (SDL_Event event; SDL_PollEvent(&event);)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            m_isRunning = false;
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            m_isMinimized = true;
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            m_isMinimized = false;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            m_renderer.OnWindowResize();
            m_camera.SetScreenSize(event.window.data1, event.window.data2);
            break;
        }
    }
}

void Application::CalcDeltaTime()
{
    static auto prevTime    = std::chrono::high_resolution_clock::now();
    const auto  currentTime = std::chrono::high_resolution_clock::now();

    m_deltaTime = std::chrono::duration<float>(currentTime - prevTime).count();

    prevTime = currentTime;
}

void Application::BuildApplicationUI()
{
    // Build custom UI here!
    // ImGui::ShowDemoWindow();

    // Debug Panel
    if (ImGui::TreeNodeEx("Util", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Show Memory Usage"))
            RenderUtils::ShowMemoryUsage(m_renderer.GetContext(), false);

        if (ImGui::Button("Show Memory Usage Detailed"))
            RenderUtils::ShowMemoryUsage(m_renderer.GetContext(), true);

        ImGui::TreePop();
    }

    // Pipelines
    if (ImGui::TreeNodeEx("Pipelines", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (auto & [name, pipeline] : m_pipelines)
        {
            ImGui::PushID(name.c_str());

            bool isActive = (name == m_activePipeline);

            if (ImGui::Checkbox(name.c_str(), &isActive))
                m_activePipeline = name;

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    // Scene Objects
    if (ImGui::TreeNodeEx("Scene Objects", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (auto & sceneObject : m_sceneObjects)
        {
            ImGui::PushID(sceneObject.GetName().c_str());

            bool isVisible = sceneObject.GetVisible();

            if (ImGui::Checkbox(sceneObject.GetName().c_str(), &isVisible))
                sceneObject.SetVisible(isVisible);

            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}
