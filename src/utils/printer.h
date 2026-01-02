#pragma once

// Base colors
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

// Extra individual colors
#define COLOR_ORANGE        "\033[38;5;208m"
#define COLOR_DARK_RED      "\033[38;5;88m"
#define COLOR_PINK          "\033[38;5;212m"
#define COLOR_LIME          "\033[38;5;154m"
#define COLOR_MINT          "\033[38;5;121m"
#define COLOR_TURQUOISE     "\033[38;5;80m"
#define COLOR_SKY_BLUE      "\033[38;5;117m"
#define COLOR_NAVY          "\033[38;5;17m"
#define COLOR_VIOLET        "\033[38;5;177m"
#define COLOR_LAVENDER      "\033[38;5;183m"
#define COLOR_BROWN         "\033[38;5;94m"
#define COLOR_GRAY          "\033[38;5;244m"
#define COLOR_DARK_GRAY     "\033[38;5;238m"
#define COLOR_GOLD          "\033[38;5;220m"

void setup_print(const char *name, const char *color);

__attribute__((format(printf, 1, 2)))
void print_msg(const char *fmt, ...);

__attribute__((format(printf, 2, 3)))
void print_msg_color(const char *color, const char *fmt, ...);

__attribute__((format(printf, 4, 5)))
void print_error_impl(const char *file, int line, const char *func,
                      const char *fmt, ...);

#define print_error(fmt, ...) \
print_error_impl(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
