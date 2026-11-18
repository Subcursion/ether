#include "ether/platform/window.h"
#include "ether/rc.h"
#include "ether/logging.h"
#include "ether/profiler.h"

#include <SDL3/SDL.h>

ether_rc ether_window_init(ether_app_t *app, const char *window_title, int width, int height)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(NULL != window_title, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(0 < width, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(0 < height, rc = ETHER_EPARAM, goto EXIT);

    ETHER_ILOG("Initializing SDL3");

    PROFILER_GET_TIMER(SDL_Init);
    PROFILER_START_TIMER(SDL_Init_timer);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
    {
        PROFILER_END_TIMER();
        ETHER_ELOGV("Failed to initialize SDL3: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }
    PROFILER_END_TIMER();

    PROFILER_GET_TIMER(SDL_CreateWindow);
    PROFILER_START_TIMER(SDL_CreateWindow_timer);

    ETHER_ILOG("Creating SDL3 Window");
    app->sdl_window = SDL_CreateWindow(window_title, width, height,
                                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    PROFILER_END_TIMER();
    if (NULL == app->sdl_window)
    {
        ETHER_ELOGV("Failed to create SDL Window: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }

EXIT:
    return rc;
}

void ether_window_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);

    if (NULL != app->sdl_window)
    {
        ETHER_ILOG("Destroying SDL Window");
        SDL_DestroyWindow(app->sdl_window);
        app->sdl_window = NULL;
    }

    SDL_Quit();
}