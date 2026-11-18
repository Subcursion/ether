#ifndef ETHER_PROFILER_H
#define ETHER_PROFILER_H

#include "ether/rc.h"
#include "ether/logging.h"
#include "ether/ansi.h"

#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* This is a pretty naive approach to logging timing for functions in the renderer. Easy to break by typing in the terminal, bad sizing, etc.
    Would recommend using ncurses
*/

/// @brief Records state of timers as a stack
typedef struct profiler_timer
{
    struct timespec start_time;
    struct timespec end_time;

    struct timespec duration;

    const char *name;

    uint32_t indent_level;

    int write_resume_col;

    struct profiler_timer *parent;

    /// @brief Used to keep track of how many elements we drew beneath a parent
    uint32_t children_rendered;

    bool started;

} profiler_timer_t;

extern profiler_timer_t *current_timer;

#define PROFILER_GET_TIMER(timer_name) \
    profiler_timer_t timer_name##_timer = {.name = #timer_name};

#define PROFILER_START_TIMER(timer)                                                         \
    do                                                                                      \
    {                                                                                       \
        if (NULL != current_timer)                                                          \
        {                                                                                   \
            timer.parent = current_timer;                                                   \
            timer.indent_level = NULL != timer.parent ? timer.parent->indent_level + 1 : 0; \
            timer.parent->children_rendered++;                                              \
        }                                                                                   \
        current_timer = &timer;                                                             \
        if (0 != clock_gettime(CLOCK_MONOTONIC, &timer.start_time))                         \
        {                                                                                   \
            ETHER_ELOGV("Failed to get profiler start time: %s", strerror(errno));          \
            break;                                                                          \
        }                                                                                   \
        timer.started = true;                                                               \
        if (NULL != timer.parent)                                                           \
        {                                                                                   \
            if (timer.parent->children_rendered > 1)                                        \
            {                                                                               \
                                                                                            \
                fprintf(stdout, CSI_CURSOR_DOWN_N, timer.parent->children_rendered - 1);    \
            }                                                                               \
            fprintf(stdout, "\n\r");                                                        \
        }                                                                                   \
        timer.write_resume_col = 1 + fprintf(stdout, "%*s%s [%lld.%.9ld] ",                 \
                                             2 * (timer.indent_level + 1), " ", /* TODO */  \
                                             timer.name,                                    \
                                             (long long)timer.start_time.tv_sec,            \
                                             timer.start_time.tv_nsec);                     \
        fflush(stdout);                                                                     \
    } while (0)

#define PROFILER_END_TIMER()                                                                                   \
    do                                                                                                         \
    {                                                                                                          \
        if (0 != clock_gettime(CLOCK_MONOTONIC, &current_timer->end_time))                                     \
        {                                                                                                      \
            ETHER_ELOGV("Failed to get profiler end time: %s", strerror(errno));                               \
            break;                                                                                             \
        }                                                                                                      \
        current_timer->started = false;                                                                        \
        current_timer->duration.tv_sec = current_timer->end_time.tv_sec - current_timer->start_time.tv_sec;    \
        current_timer->duration.tv_nsec = current_timer->end_time.tv_nsec - current_timer->start_time.tv_nsec; \
        if (current_timer->duration.tv_nsec < 0)                                                               \
        {                                                                                                      \
            current_timer->duration.tv_sec--;                                                                  \
            current_timer->duration.tv_nsec += 1000000000L;                                                    \
        }                                                                                                      \
        fprintf(stdout, "[%lld.%.9ld] %lld.%.9ld\n\r",                                                         \
                (long long)current_timer->end_time.tv_sec,                                                     \
                current_timer->end_time.tv_nsec,                                                               \
                (long long)current_timer->duration.tv_sec,                                                     \
                current_timer->duration.tv_nsec);                                                              \
        if (current_timer->children_rendered > 0)                                                              \
        {                                                                                                      \
            fprintf(stdout, CSI_CURSOR_DOWN_N, current_timer->children_rendered);                              \
        }                                                                                                      \
                                                                                                               \
        if (current_timer->parent != NULL)                                                                     \
        {                                                                                                      \
            current_timer->parent->children_rendered += current_timer->children_rendered;                      \
                                                                                                               \
            fprintf(stdout, CSI_CURSOR_UP_N CSI_CURSOR_HORIZONTAL_ABSOLUTE,                                    \
                    current_timer->parent->children_rendered + 1,                                              \
                    current_timer->parent->write_resume_col);                                                  \
        }                                                                                                      \
        fflush(stdout);                                                                                        \
        current_timer = current_timer->parent;                                                                 \
    } while (0)

#endif