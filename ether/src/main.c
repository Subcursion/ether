
#include "ether/logging.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    FILE *log_stream = fopen("./log.txt", "a+");
    if (NULL == log_stream)
    {
        fprintf(stderr, "Failed to open log `%s` for writing.\n", "./log.txt");
        return -1;
    }

    ether_log_init(log_stream, ETHER_LOG_INFO);

    ether_log(ETHER_LOG_INFO, "We are up and working");

    return ether_log_close(log_stream);
}