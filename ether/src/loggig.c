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

#define _LOG_FORMAT "[%c] <%ld> %s:%d | %s > "

#define _ILOG_CHAR 'I'
#define _WLOG_CHAR 'W'
#define _ELOG_CHAR 'E'

atomic_bool _log_init = false;
FILE *log_stream = NULL;
ether_log_level_e min_log_level = ETHER_LOG_WARN;

pthread_mutex_t log_stream_mutex = PTHREAD_MUTEX_INITIALIZER;

ether_rc ether_log_init(FILE *stream, ether_log_level_e min_level)
{
    ether_rc rc = ETHER_SUCCESS;

    if (NULL == stream)
    {
        stream = fopen(DEFAULT_LOG_PATH, "r+");
        if (NULL == stream)
        {
            ether_log(ETHER_LOG_ERROR, "Failed to open `%s` for logging");
            return errno;
        }
    }

    pthread_mutex_lock(&log_stream_mutex);

    log_stream = stream;
    min_log_level = min_level;
    _log_init = true;

    pthread_mutex_unlock(&log_stream_mutex);

    return rc;
}

ether_rc ether_log_close(FILE *stream)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != stream, rc = ETHER_EPARAM, return rc);

    pthread_mutex_lock(&log_stream_mutex);
    fprintf(log_stream, "%s\n", "-----");
    _log_init = false;
    pthread_mutex_unlock(&log_stream_mutex);

    return fclose(stream);
}

void ether_log(ether_log_level_e level, const char *fmt, ...)
{
    if (!_log_init)
    {
        ether_log_init(NULL, ETHER_LOG_INFO);
    }
    pthread_mutex_lock(&log_stream_mutex);

    if (level < min_log_level)
    {
        return;
    }

    char level_char = _ILOG_CHAR;
    if (level == ETHER_LOG_WARN)
    {
        level_char = _WLOG_CHAR;
    }
    else if (level == ETHER_LOG_ERROR)
    {
        level_char = _ELOG_CHAR;
    }
    else
    {
        level_char = '?';
    }

    va_list ap;

    va_start(ap, fmt);

    fprintf(log_stream, _LOG_FORMAT, level_char, time(NULL), __FILE__, __LINE__, __func__);
    vfprintf(log_stream, fmt, ap);
    fprintf(log_stream, "%s", "\n");

    va_end(ap);

    pthread_mutex_unlock(&log_stream_mutex);
}
