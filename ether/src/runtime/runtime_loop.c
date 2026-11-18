#include "ether/runtime/runtime_loop.h"
#include "ether/rc.h"
#include "ether/rendering/render.h"

#include <SDL3/SDL.h>
#include <time.h>
#include <unistd.h>

ether_rc ether_engine_loop(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);

    ETHER_ILOG("Entering main engine loop");

    while (!app->should_quit)
    {
        rc = ETHER_SUCCESS;
        // poll for events
        while (SDL_PollEvent(&app->event))
        {
            switch (app->event.type)
            {
            case SDL_EVENT_QUIT:
                ETHER_ILOG("SDL Quit Event received");
                app->should_quit = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                ETHER_ILOG("SDL Resize Event");
                app->pending_swapchain_rebuild = true;
                break;
            default:
                // nothing to do
                break;
            }
        }

        if (app->should_quit)
            break;

        // update scene state

        // render
        rc = ether_render(app);
        if (ETHER_SUCCESS != rc)
        {
            if (ETHER_DELAY == rc)
            {
                // sleep and try again next loop
                sleep(1); // TODO[future] do a more precise sleep
                continue;
            }
            app->should_quit = true;
            break;
        }
    }

    ETHER_ILOG("Main engine loop exited");

EXIT:
    return rc;
}