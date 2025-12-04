#include "printer.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>


void print_msg(const char* process_name, const char* color, const char* fmt, ...) {
    char buffer[1024];
    char msg_buffer[800];
    char time_buffer[9];  // This is minimal size for format HH:MM:SS/0


    time_t rawtime = time(NULL);
    struct tm* timeinfo = localtime(&rawtime);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", timeinfo);


    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);


    pid_t pid = getpid();

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "%s[%s] [%s (PID=%d)] %s%s\n",
        color,
        time_buffer,
        process_name,
        pid,
        msg_buffer,
        COLOR_RESET
    );

    write(STDOUT_FILENO, buffer, len);
}