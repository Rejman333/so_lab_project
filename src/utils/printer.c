#include "printer.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

#define BUFFER_SIZE 1024

static const char *PROCESS_NAME = "Unknown";
static const char *PROCESS_COLOR = "\033[0m";
static int PROCESS_PID = -1;

void setup_print(const char *name, const char *color) {
    PROCESS_NAME = name;
    PROCESS_COLOR = color;
    PROCESS_PID = getpid();
}

void print_internal(const char *color, const char *fmt, va_list args) {
    char buffer[BUFFER_SIZE];
    char time_buffer[9];


    time_t rawtime = time(NULL);
    struct tm timeinfo;
    localtime_r(&rawtime, &timeinfo);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &timeinfo);

    int pos = snprintf(
        buffer,
        BUFFER_SIZE,
        "%s[%s] [%s (PID=%d)] ",
        color,
        time_buffer,
        PROCESS_NAME,
        PROCESS_PID
    );

    pos += vsnprintf(
        buffer + pos,
        BUFFER_SIZE - pos,
        fmt,
        args
    );

    pos += snprintf(
        buffer + pos,
        BUFFER_SIZE - pos,
        "\033[0m\n"
    );

    if (pos < 0) {
        pos = 0;
    } else if (pos >= BUFFER_SIZE) {
        pos = BUFFER_SIZE - 1;
    }

    write(STDOUT_FILENO, buffer, pos);
}

void print_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(PROCESS_COLOR, fmt, args);
    va_end(args);
}

void print_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(COLOR_RED, fmt, args);
    va_end(args);
};

void print_msg_color(const char *color, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(color, fmt, args);
    va_end(args);
}
