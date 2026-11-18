
#include "ether/logging.h"
#include "ether/app.h"
#include "ether/platform/window.h"
#include "ether/rendering/vulkan_state.h"
#include "ether/runtime/runtime_loop.h"

#include "ether/ansi.h"
#include "ether/profiler.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

ether_app_t application = {0};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ether_rc rc = ETHER_SUCCESS;

    // swap to alternate display and clear the screen
    fprintf(stdout, CSI_ENABLE_ALTERNATE_BUFFER CSI_ERASE_DISPLAY(ERASE_ENTIRE_AND_HISTORY) CSI_HIDE_CURSOR);
    fflush(stdout);

    PROFILER_GET_TIMER(ether_log_init);
    PROFILER_START_TIMER(ether_log_init_timer);

    FILE *log_stream = fopen("./log.txt", "a+");
    if (NULL == log_stream)
    {
        fprintf(stderr, "Failed to open log `%s` for writing.\n", "./log.txt");
        return -1;
    }

    ether_log_init(log_stream, ETHER_LOG_INFO);

    PROFILER_END_TIMER();

    PROFILER_GET_TIMER(ether_window_init);
    PROFILER_START_TIMER(ether_window_init_timer);

    if (ETHER_SUCCESS != (rc = ether_window_init(&application, "Ether Tracer", 800, 600)))
    {
        goto CLEANUP;
    }
    PROFILER_END_TIMER();

    PROFILER_GET_TIMER(ether_vulkan_init);
    PROFILER_START_TIMER(ether_vulkan_init_timer);
    if (ETHER_SUCCESS != (rc = ether_vulkan_init(&application)))
    {
        goto CLEANUP;
    }

    PROFILER_END_TIMER();

    PROFILER_GET_TIMER(ether_engine_loop);
    PROFILER_START_TIMER(ether_engine_loop_timer);
    if (ETHER_SUCCESS != (rc = ether_engine_loop(&application)))
    {
        PROFILER_END_TIMER();
        goto CLEANUP;
    }
    PROFILER_END_TIMER();

CLEANUP:
    ETHER_ILOGV("Ether stopped with status: %s", ether_rc_str(rc));

    ether_vulkan_destroy(&application);
    ether_window_destroy(&application);

    if (ETHER_SUCCESS != ether_log_close())
    {
        // not good.
        if (ETHER_SUCCESS == rc)
        {
            rc = errno;
        }
    }

    // restore the display
    fprintf(stdout, CSI_SHOW_CURSOR CSI_DISABLE_ALTERNATE_BUFFER);
    fflush(stdout);

    return rc == ETHER_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}