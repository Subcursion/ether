#include "ether/logging.h"
#include "ether/rc.h"

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define DEFAULT_LOG_PATH "./log.txt"

const char ether_log_level_chars[] = {
    'I', 'W', 'E'};

ether_log_level_e ether_min_log_level = ETHER_LOG_WARN;

pthread_mutex_t ether_log_lock = PTHREAD_MUTEX_INITIALIZER;

FILE *log_stream = NULL;

ether_rc ether_log_init(const char *log_path, ether_log_level_e min_level)
{
    ether_rc rc = ETHER_SUCCESS;

    ether_min_log_level = min_level;

    if (NULL == (log_stream = fopen(log_path, "a+")))
    {
        fprintf(stderr, "Failed to open log: %s\n", strerror(errno));
        return ETHER_EPARAM;
    }

    // just stderr and make it point at the new log stream

    if (-1 == dup2(fileno(log_stream), STDERR_FILENO))
    {
        fprintf(stderr, "Failed to duplicate stderr to log stream: %s\n", strerror(errno));
        return ETHER_EPARAM;
    }

    // enable line buffering mode
    if (setvbuf(log_stream, NULL, _IOLBF, 0) != 0)
    {
        fprintf(stderr, "Failed to configure log line buffering\n");
        return ETHER_INIT;
    }

    return rc;
}

ether_rc ether_log_close(void)
{
    if (log_stream == NULL)
        return ETHER_SUCCESS;

    fprintf(log_stream, "%s\n", "-----");
    int result = fclose(log_stream);
    log_stream = NULL;
    return result;
}
