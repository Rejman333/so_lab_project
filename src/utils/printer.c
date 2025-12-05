#include "printer.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

static const char* PROCESS_NAME = "Unknown";
static const char* PROCESS_COLOR = "\033[0m";

void setup_print(const char* name, const char* color) {
    PROCESS_NAME = name;
    PROCESS_COLOR = color;
}

static void print_internal(const char* color, const char* fmt, va_list args) {
    char buffer[1024];
    char msg_buffer[800];
    char time_buffer[9];

    time_t rawtime = time(NULL);
    struct tm* timeinfo = localtime(&rawtime);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", timeinfo);

    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);

    pid_t pid = getpid();

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "%s[%s] [%s (PID=%d)] %s\033[0m\n",
        color,
        time_buffer,
        PROCESS_NAME,
        pid,
        msg_buffer
    );

    write(STDOUT_FILENO, buffer, len);
}

void print_msg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(PROCESS_COLOR, fmt, args);
    va_end(args);
}

//ToDO Remake
void print_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(PROCESS_COLOR, fmt, args);
    va_end(args);
};

void print_msg_color(const char* color, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(color, fmt, args);
    va_end(args);
}
