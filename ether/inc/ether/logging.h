#ifndef ETHER_LOGGING_H
#define ETHER_LOGGING_H

#include "ether/rc.h"

#include <stdio.h>
#include <pthread.h>

typedef enum
{
    ETHER_LOG_INFO,
    ETHER_LOG_WARN,
    ETHER_LOG_ERROR
} ether_log_level_e;

extern ether_log_level_e ether_min_log_level;
extern const char ether_log_level_chars[];
extern pthread_mutex_t ether_log_lock;
extern FILE *ether_log_stream;

ether_rc ether_log_init(FILE *stream, ether_log_level_e min_level);
ether_rc ether_log_close(FILE *stream);

#define _LOG_FORMAT "[%c] <%ld> %s:%d | %s > "

/// @brief Acquires log lock and prints log starting statement, leaving caller able to print to log as desired.
/// Use `ETHER_EXIT_LOG()` to finish the log statement
#define ETHER_ENTER_LOG(level)                                                                            \
    do                                                                                                    \
    {                                                                                                     \
        ether_log_init(NULL, ETHER_LOG_WARN);                                                             \
        if (level >= ether_min_log_level)                                                                 \
        {                                                                                                 \
            pthread_mutex_lock(&ether_log_lock);                                                          \
                                                                                                          \
            char level_char = ether_log_level_chars[level];                                               \
                                                                                                          \
            fprintf(ether_log_stream, _LOG_FORMAT, level_char, time(NULL), __FILE__, __LINE__, __func__); \
        }                                                                                                 \
    } while (0)

/// @brief Prints a newline (\n) and flushes the log stream, then releases the lock.
/// Must be started with `ETHER_ENTER_LOG(level)`
#define ETHER_EXIT_LOG()                       \
    do                                         \
    {                                          \
        fprintf(ether_log_stream, "%s", "\n"); \
        fflush(ether_log_stream);              \
                                               \
        pthread_mutex_unlock(&ether_log_lock); \
    } while (0)

#define ETHER_LOGV(level, fmt, ...)                    \
    do                                                 \
    {                                                  \
        ETHER_ENTER_LOG(level);                        \
        fprintf(ether_log_stream, fmt, ##__VA_ARGS__); \
        ETHER_EXIT_LOG();                              \
    } while (0)

#define ETHER_LOG(level, msg) ETHER_LOGV(level, "%s", msg)

#define ETHER_ILOG(msg) ETHER_LOG(ETHER_LOG_INFO, msg)
#define ETHER_ILOGV(fmt, ...) ETHER_LOGV(ETHER_LOG_INFO, fmt, ##__VA_ARGS__)
#define ETHER_WLOG(msg) ETHER_LOG(ETHER_LOG_WARN, msg)
#define ETHER_WLOGV(fmt, ...) ETHER_LOGV(ETHER_LOG_WARN, fmt, ##__VA_ARGS__)
#define ETHER_ELOG(msg) ETHER_LOG(ETHER_LOG_ERROR, msg)
#define ETHER_ELOGV(fmt, ...) ETHER_LOGV(ETHER_LOG_ERROR, fmt, ##__VA_ARGS__)

#endif