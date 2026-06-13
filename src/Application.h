#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "Renderer.h"
#include "UIRenderer.h"
#include "Camera.h"

class SceneObject;
class RenderPipeline;

struct SDL_Window;

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
