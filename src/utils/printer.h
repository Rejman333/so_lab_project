#pragma once

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"


void setup_print(const char* name, const char* color);

__attribute__((format(printf, 1, 2)))
void print_msg(const char* fmt, ...);

__attribute__((format(printf, 1, 2)))
void print_error(const char* fmt, ...);

__attribute__((format(printf, 2, 3)))
void print_msg_color(const char* color, const char* fmt, ...);