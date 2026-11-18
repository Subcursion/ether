#include "ether/runtime/runtime_loop.h"
#include "ether/rc.h"
#include "ether/rendering/render.h"
#include "ether/profiler.h"
#include "ether/ansi.h"

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
        PROFILER_GET_TIMER(engine_loop);
        current_timer->children_rendered = 0; // reset so we can re-render the frame timings
        // clear display
        // fputs(CSI_SAVE_CURSOR CSI_CURSOR_NEXT_LINE
        //           CSI_ERASE_DISPLAY(ERASE_TO_END) CSI_RESTORE_CURSOR,
        //       stdout);
        PROFILER_START_TIMER(engine_loop_timer);
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
            goto RESTART_LOOP; // will exit though

        // update scene state

        // render
        PROFILER_GET_TIMER(ether_render);
        PROFILER_START_TIMER(ether_render_timer);
        rc = ether_render(app);
        PROFILER_END_TIMER();
        if (ETHER_SUCCESS != rc)
        {
            if (ETHER_DELAY == rc)
            {
                // sleep and try again next loop
                sleep(1); // TODO[future] do a more precise sleep
                goto RESTART_LOOP;
            }
            app->should_quit = true;
            goto RESTART_LOOP;
        }

    RESTART_LOOP:
        PROFILER_END_TIMER();
        // need to reset the view to be the same line as the log instead of the next one
        // fprintf(stdout, CSI_CURSOR_PREVIOUS_LINE_N CSI_ERASE_DISPLAY(ERASE_TO_END),
        //         1 + (int)engine_loop_timer.children_rendered);
        // fflush(stdout);
    }

    ETHER_ILOG("Main engine loop exited");

EXIT:
    return rc;
}