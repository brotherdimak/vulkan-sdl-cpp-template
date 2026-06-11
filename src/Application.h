#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <unordered_map>
#include <vector>
#include <string>

#include "Camera.h"
#include "RenderPipeline.h"
#include "RenderUtils.h"
#include "Renderer.h"
#include "SceneObject.h"
#include "UIRenderer.h"

class Application
{
public:
    Application();
    ~Application();

    void Init();
    void Run();

private:
    void InitCamera();
    void InitPipelines();
    void InitSceneObjects();

    void HandleEvents();
    void CalcDeltaTime();
    void BuildApplicationUI();

private:
    bool  m_isRunning;
    bool  m_isMinimized;
    float m_deltaTime;

    SDL_Window * m_window;

    Renderer   m_renderer;
    UIRenderer m_uiRenderer;
    Camera     m_camera;

    std::string                                     m_activePipeline;
    std::unordered_map<std::string, RenderPipeline> m_pipelines;
    std::vector<SceneObject>                        m_sceneObjects;
};
