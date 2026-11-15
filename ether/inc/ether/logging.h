#ifndef ETHER_LOGGING_H
#define ETHER_LOGGING_H

#include "ether/rc.h"

#include <stdio.h>

typedef enum
{
    ETHER_LOG_INFO,
    ETHER_LOG_WARN,
    ETHER_LOG_ERROR
} ether_log_level_e;

ether_rc ether_log_init(FILE *stream, ether_log_level_e min_level);
ether_rc ether_log_close(FILE *stream);

void ether_log(ether_log_level_e level, const char *fmt, ...);

#endif