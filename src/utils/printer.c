#include "printer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#define EINTR 4

#define BUFFER_SIZE 1024

static const char *PROCESS_NAME = "Unknown";
static const char *PROCESS_COLOR = "\033[0m";
static int PROCESS_PID = -1;


static int log_file_descriptor = -1;


static int lock_log_file_for_writing() {
    struct flock file_lock = {0};

    file_lock.l_type   = F_WRLCK;
    file_lock.l_whence = SEEK_SET;
    file_lock.l_start  = 0;
    file_lock.l_len    = 0;

    while (fcntl(log_file_descriptor, F_SETLKW, &file_lock) == -1) {
        if (errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}

static int unlock_log_file() {
    struct flock file_lock = {0};

    file_lock.l_type   = F_UNLCK;
    file_lock.l_whence = SEEK_SET;
    file_lock.l_start  = 0;
    file_lock.l_len    = 0;

    while (fcntl(log_file_descriptor, F_SETLK, &file_lock) == -1) {
        if (errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}


int global_logger_initialize(const char* log_file_path) {
    if (!log_file_path) return -1;

    log_file_descriptor = open(
        log_file_path,
        O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC,
        0600
    );
    return (log_file_descriptor < 0) ? -1 : 0;
}

int logger_initialize(const char* log_file_path) {
    if (!log_file_path) return 0;

    log_file_descriptor = open(
        log_file_path,
        O_WRONLY | O_CREAT | O_APPEND| O_CLOEXEC,
        0600
    );
    return (log_file_descriptor < 0) ? -1 : 0;
}

void logger_shutdown() {
    if (log_file_descriptor >= 0) {
        close(log_file_descriptor);
        log_file_descriptor = -1;
    }
}


void setup_print(const char *name, const char *color) {
    PROCESS_NAME = name;
    PROCESS_COLOR = color;
    PROCESS_PID = getpid();
}

void print_internal(int output, const char *color, const char *fmt, va_list args) {
    char console_buffer[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    char time_buffer[9];

    time_t rawtime = time(NULL);
    struct tm timeinfo;
    localtime_r(&rawtime, &timeinfo);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &timeinfo);

    va_list args_copy;
    va_copy(args_copy, args);

    int console_pos = snprintf(
        console_buffer,
        BUFFER_SIZE,
        "%s[%s] [%s (PID=%d)] ",
        color,
        time_buffer,
        PROCESS_NAME,
        PROCESS_PID
    );

    console_pos += vsnprintf(
        console_buffer + console_pos,
        BUFFER_SIZE - console_pos,
        fmt,
        args
    );

    console_pos += snprintf(
        console_buffer + console_pos,
        BUFFER_SIZE - console_pos,
        "\033[0m\n"
    );

    if (console_pos < 0) {
        console_pos = 0;
    } else if (console_pos >= BUFFER_SIZE) {
        console_pos = BUFFER_SIZE - 1;
    }

    write(output, console_buffer, console_pos);

    if (log_file_descriptor >= 0) {
        int file_pos = snprintf(
            file_buffer,
            BUFFER_SIZE,
            "[%s] [%s (PID=%d)] ",
            time_buffer,
            PROCESS_NAME,
            PROCESS_PID
        );

        file_pos += vsnprintf(
            file_buffer + file_pos,
            BUFFER_SIZE - file_pos,
            fmt,
            args_copy
        );

        file_pos += snprintf(
            file_buffer + file_pos,
            BUFFER_SIZE - file_pos,
            "\n"
        );

        if (file_pos < 0) {
            file_pos = 0;
        } else if (file_pos >= BUFFER_SIZE) {
            file_pos = BUFFER_SIZE - 1;
        }

        if (lock_log_file_for_writing() == 0) {
            write(log_file_descriptor, file_buffer, file_pos);
            unlock_log_file();
        }
    }

    va_end(args_copy);
}

void print_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_internal(STDOUT_FILENO, PROCESS_COLOR, fmt, args);
    va_end(args);
}

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
