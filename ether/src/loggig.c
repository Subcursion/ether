#include "ether/logging.h"
#include "ether/rc.h"

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#define DEFAULT_LOG_PATH "./log.txt"

const char ether_log_level_chars[] = {
    'I', 'W', 'E'};

atomic_bool _log_init = false;
FILE *ether_log_stream = NULL;
ether_log_level_e ether_min_log_level = ETHER_LOG_WARN;

pthread_mutex_t ether_log_lock = PTHREAD_MUTEX_INITIALIZER;

ether_rc ether_log_init(FILE *stream, ether_log_level_e min_level)
{
    ether_rc rc = ETHER_SUCCESS;

    if (_log_init)
        return ETHER_SUCCESS;

    if (NULL == stream)
    {
        stream = fopen(DEFAULT_LOG_PATH, "r+");
        if (NULL == stream)
        {
            ETHER_ELOGV("Failed to open `%s` for logging", DEFAULT_LOG_PATH);
            return errno;
        }
    }

    pthread_mutex_lock(&ether_log_lock);

    ether_log_stream = stream;
    ether_min_log_level = min_level;
    _log_init = true;

    pthread_mutex_unlock(&ether_log_lock);

    return rc;
}

ether_rc ether_log_close(FILE *stream)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != stream, rc = ETHER_EPARAM, return rc);

    pthread_mutex_lock(&ether_log_lock);
    fprintf(ether_log_stream, "%s\n", "-----");
    _log_init = false;
    pthread_mutex_unlock(&ether_log_lock);

    return fclose(stream);
}
