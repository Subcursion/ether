#ifndef ETHER_LOGGING_H
#define ETHER_LOGGING_H

#include "ether/rc.h"

#include <stdio.h>
#include <time.h>

typedef enum
{
    ETHER_LOG_INFO,
    ETHER_LOG_WARN,
    ETHER_LOG_ERROR
} ether_log_level_e;

extern ether_log_level_e ether_min_log_level;
extern const char ether_log_level_chars[];
extern FILE *ether_log_stream;

ether_rc ether_log_init(FILE *stream, ether_log_level_e min_level);
ether_rc ether_log_close(void);

#define _LOG_FORMAT "[%c] <%ld> %s:%d | %s > "

#define ETHER_LOG_RAWV(level, fmt, ...)                \
    do                                                 \
    {                                                  \
        if (level < ether_min_log_level)               \
            break;                                     \
        fprintf(ether_log_stream, fmt, ##__VA_ARGS__); \
    } while (0)

#define ETHER_LOGV(level, fmt, ...)                                                                   \
    do                                                                                                \
    {                                                                                                 \
        if (level < ether_min_log_level)                                                              \
            break;                                                                                    \
        char level_char = ether_log_level_chars[level];                                               \
        fprintf(ether_log_stream, _LOG_FORMAT, level_char, time(NULL), __FILE__, __LINE__, __func__); \
        fprintf(ether_log_stream, fmt "\n", ##__VA_ARGS__);                                           \
    } while (0)

#define ETHER_LOG_RAW(level, msg) ETHER_LOG_RAWV(level, "%s", msg)

#define ETHER_LOG(level, msg) ETHER_LOGV(level, "%s", msg)

#define ETHER_ILOG(msg) ETHER_LOG(ETHER_LOG_INFO, msg)
#define ETHER_ILOGV(fmt, ...) ETHER_LOGV(ETHER_LOG_INFO, fmt, ##__VA_ARGS__)
#define ETHER_WLOG(msg) ETHER_LOG(ETHER_LOG_WARN, msg)
#define ETHER_WLOGV(fmt, ...) ETHER_LOGV(ETHER_LOG_WARN, fmt, ##__VA_ARGS__)
#define ETHER_ELOG(msg) ETHER_LOG(ETHER_LOG_ERROR, msg)
#define ETHER_ELOGV(fmt, ...) ETHER_LOGV(ETHER_LOG_ERROR, fmt, ##__VA_ARGS__)

#endif