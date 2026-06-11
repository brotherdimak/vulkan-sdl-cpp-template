#include "Application.h"
#include <SDL3/SDL.h>
#include <stdexcept>

int main()
{
    try
    {
        Application app;

        app.Init();
        app.Run();
    }
    catch (const std::runtime_error & e)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
        SDL_Quit();

        return EXIT_FAILURE;
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}