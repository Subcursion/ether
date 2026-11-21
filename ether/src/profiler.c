#include "ether/profiler.h"
#include "ether/ansi.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#ifdef ETHER_PROFILER_LIBDW
#include <elfutils/libdwfl.h>
#endif

#define PROFILER_MAX_CALLS UINT16_MAX
#define NO_INSTRUMENT __attribute__((no_instrument_function))

typedef struct profile_call
{
    void *function;
    struct timespec start;
    struct timespec end;
    struct profile_call *parent;
    unsigned int depth;
    bool started;
    bool complete;
} profile_call;

static profile_call calls[PROFILER_MAX_CALLS];
static unsigned int call_count;
static unsigned int frame_start;
static unsigned int printed_count;
static bool print_prefix;
static bool terminal_output;
static int frame_rows;
static int frame_parent_rows;
static unsigned int depth;
static profile_call *current;
static const profile_call *frame_parent;
static _Thread_local bool capturing;

void NO_INSTRUMENT __cyg_profile_func_enter(void *func, void *caller);
void NO_INSTRUMENT __cyg_profile_func_exit(void *func, void *caller);

void __cyg_profile_func_enter(void *func, void *caller)
{
    (void)caller;
    if (!capturing)
        return;

    unsigned int call_depth = depth++;
    if (call_count == PROFILER_MAX_CALLS)
        return;

    profile_call *call = &calls[call_count++];
    call->function = func;
    call->depth = call_depth;
    call->parent = current;
    call->complete = false;
    current = call;
    call->started = clock_gettime(CLOCK_MONOTONIC, &call->start) == 0;
}

void __cyg_profile_func_exit(void *func, void *caller)
{
    (void)caller;
    if (!capturing || depth == 0)
        return;

    --depth;
    profile_call *call = current;
    if (call == NULL || call->depth != depth || call->function != func)
        return;

    call->complete = call->started && clock_gettime(CLOCK_MONOTONIC, &call->end) == 0;
    current = call->parent;
}

void NO_INSTRUMENT profiler_start(void)
{
    terminal_output = isatty(STDOUT_FILENO) != 0;
    capturing = true;
}

void NO_INSTRUMENT profiler_frame_checkpoint(void)
{
    frame_start = call_count;
    frame_parent = current != NULL && current->depth + 1 == depth ? current : NULL;
    print_prefix = true;
}

void NO_INSTRUMENT profiler_frame_begin(void)
{
    // The enclosing calls are still active. Only rewind the recorded history.
    call_count = frame_start;
}

static const char *profile_function_name(void *function)
{
    Dl_info info;
    if (dladdr(function, &info) != 0 && info.dli_saddr == function && info.dli_sname != NULL)
        return info.dli_sname;

#ifdef ETHER_PROFILER_LIBDW
    static const Dwfl_Callbacks callbacks = {
        .find_elf = dwfl_linux_proc_find_elf,
    };
    static Dwfl *symbols;
    static bool initialized;
    if (!initialized)
    {
        initialized = true;
        symbols = dwfl_begin(&callbacks);
        if (symbols != NULL &&
            (dwfl_linux_proc_report(symbols, getpid()) != 0 ||
             dwfl_report_end(symbols, NULL, NULL) != 0))
        {
            dwfl_end(symbols);
            symbols = NULL;
        }
    }

    if (symbols != NULL)
    {
        Dwarf_Addr address = (uintptr_t)function;
        Dwfl_Module *module = dwfl_addrmodule(symbols, address);
        if (module != NULL)
        {
            GElf_Off offset;
            GElf_Sym symbol;
            const char *name = dwfl_module_addrinfo(module, address, &offset, &symbol,
                                                    NULL, NULL, NULL);
            if (name != NULL && offset == 0)
                return name;
        }
    }
#endif

    return NULL;
}

static int profile_print_calls(unsigned int begin, unsigned int end, int columns)
{
    int rows = 0;
    for (unsigned int idx = begin; idx < end; idx++)
    {
        const profile_call *call = &calls[idx];
        if (!call->complete && call != frame_parent)
            continue;

        const char *name = profile_function_name(call->function);
        char address[sizeof(void *) * 2 + 3];
        if (name == NULL)
        {
            snprintf(address, sizeof(address), "%p", call->function);
            name = address;
        }

        int written;
        if (!call->complete)
        {
            if (call->started)
                written = fprintf(stdout, "%*s%s [%lld.%09ld] [running]\n",
                                  (int)call->depth * 2, "", name,
                                  (long long)call->start.tv_sec, call->start.tv_nsec);
            else
                written = fprintf(stdout, "%*s%s [not started]\n", (int)call->depth * 2, "", name);
            if (written > 1)
                rows += (written - 2) / columns + 1;
            continue;
        }

        struct timespec elapsed = {
            .tv_sec = call->end.tv_sec - call->start.tv_sec,
            .tv_nsec = call->end.tv_nsec - call->start.tv_nsec,
        };
        if (elapsed.tv_nsec < 0)
        {
            elapsed.tv_sec--;
            elapsed.tv_nsec += 1000000000L;
        }

        written = fprintf(stdout, "%*s%s [%lld.%09ld] [%lld.%09ld] %lld.%09ld\n",
                          (int)call->depth * 2, "", name,
                          (long long)call->start.tv_sec, call->start.tv_nsec,
                          (long long)call->end.tv_sec, call->end.tv_nsec,
                          (long long)elapsed.tv_sec, elapsed.tv_nsec);
        if (written > 1)
            rows += (written - 2) / columns + 1;
    }
    return rows;
}

void NO_INSTRUMENT profiler_frame_end(void)
{
    bool was_capturing = capturing;
    capturing = false;

    struct winsize size = {0};
    if (terminal_output)
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    int columns = size.ws_col > 0 ? size.ws_col : 80;

    if (print_prefix)
    {
        unsigned int live_start = frame_parent != NULL
                                      ? (unsigned int)(frame_parent - calls)
                                      : frame_start;
        profile_print_calls(0, live_start, columns);
        frame_parent_rows = profile_print_calls(live_start, frame_start, columns);
        print_prefix = false;
    }

    if (frame_start < call_count)
    {
        if (terminal_output)
        {
            int previous_rows = frame_rows;
            if (size.ws_row > 0 && previous_rows >= size.ws_row)
                previous_rows = size.ws_row - 1;
            if (previous_rows > 0)
                fprintf(stdout, CSI_CURSOR_PREVIOUS_LINE_N CSI_ERASE_DISPLAY(ERASE_TO_END),
                        previous_rows);
        }

        frame_rows = profile_print_calls(frame_start, call_count, columns);
    }
    // An empty iteration leaves the previous frame visible and its records intact.
    else if (terminal_output && frame_start < printed_count)
    {
        call_count = printed_count;
    }
    printed_count = call_count;
    fflush(stdout);
    capturing = was_capturing;
}

void NO_INSTRUMENT profiler_finish(void)
{
    capturing = false;

    unsigned int first = printed_count;
    if (frame_parent != NULL && frame_parent->complete)
    {
        unsigned int parent_index = (unsigned int)(frame_parent - calls);
        if (parent_index < printed_count)
        {
            struct winsize size = {0};
            int live_rows = frame_parent_rows + frame_rows;
            if (terminal_output && live_rows > 0 &&
                ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && live_rows < size.ws_row)
            {
                fprintf(stdout, CSI_CURSOR_PREVIOUS_LINE_N CSI_ERASE_DISPLAY(ERASE_TO_END),
                        live_rows);
                first = parent_index;
            }
            else
            {
                profile_print_calls(parent_index, parent_index + 1, 80);
            }
        }
    }

    profile_print_calls(first, call_count, 80);
    fflush(stdout);
}
