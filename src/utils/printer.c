#include "printer.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#define BUFFER_SIZE 1024

static const char *PROCESS_NAME = "Unknown";
static const char *PROCESS_COLOR = "\033[0m";
static int PROCESS_PID = -1;

void setup_print(const char *name, const char *color) {
    PROCESS_NAME = name;
    PROCESS_COLOR = color;
    PROCESS_PID = getpid();
}

void print_internal(int output, const char *color, const char *fmt, va_list args) {
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

    write(output, buffer, pos);
}

void print_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(STDOUT_FILENO, PROCESS_COLOR, fmt, args);
    va_end(args);
}

// void print_error(const char *fmt, ...) {
//     const int error_code = errno;
//
//     char new_fmt[BUFFER_SIZE];
//     snprintf(new_fmt, sizeof(new_fmt), "%s: %s", fmt, strerror(error_code));
//
//     va_list args;
//     va_start(args, fmt);
//     print_internal(STDERR_FILENO, COLOR_RED, new_fmt, args);
//     va_end(args);
// }

void print_error_impl(const char *file, int line, const char *func, const char *fmt, ...) {
    const int error_code = errno;

    char new_fmt[BUFFER_SIZE];
    snprintf(new_fmt, sizeof(new_fmt),
             "%s:%d %s() | %s: %s",
             file, line, func, fmt, strerror(error_code));

    va_list args;
    va_start(args, fmt);
    print_internal(STDERR_FILENO, COLOR_RED, new_fmt, args);
    va_end(args);
}


void print_msg_color(const char *color, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(STDOUT_FILENO, color, fmt, args);
    va_end(args);
}
