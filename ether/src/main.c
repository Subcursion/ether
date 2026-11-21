
#include "ether/logging.h"
#include "ether/app.h"
#include "ether/platform/window.h"
#include "ether/rendering/vulkan_state.h"
#include "ether/runtime/runtime_loop.h"

#include "ether/ansi.h"
#include "ether/profiler.h"

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

ether_app_t application = {0};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    profiler_start();

    ether_rc rc = ETHER_SUCCESS;

    bool terminal_output = isatty(STDOUT_FILENO) != 0;
    if (terminal_output)
    {
        fputs(CSI_HIDE_CURSOR, stdout);
        fflush(stdout);
    }

    rc = ether_log_init("./log.txt", ETHER_LOG_INFO);
    if (ETHER_SUCCESS != rc)
        goto RESTORE_DISPLAY;

    if (ETHER_SUCCESS != (rc = ether_window_init(&application, "Ether Tracer", 800, 600)))
    {
        goto CLEANUP;
    }
    if (ETHER_SUCCESS != (rc = ether_vulkan_init(&application)))
    {
        goto CLEANUP;
    }

    if (ETHER_SUCCESS != (rc = ether_engine_loop(&application)))
    {
        goto CLEANUP;
    }

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

RESTORE_DISPLAY:
    profiler_finish();
    if (terminal_output)
    {
        fputs(CSI_SHOW_CURSOR, stdout);
        fflush(stdout);
    }

    return rc == ETHER_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}