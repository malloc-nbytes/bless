#ifndef COLOR_H
#define COLOR_H

// Fourground Colors
#define YELLOW        "\033[93m"
#define GREEN         "\033[32m"
#define BRIGHT_GREEN  "\033[92m"
#define GRAY          "\033[90m"
#define RED           "\033[31m"
#define BLUE          "\033[94m"
#define CYAN          "\033[96m"
#define MAGENTA       "\033[95m"
#define WHITE         "\033[97m"
#define BLACK         "\033[30m"
#define CYAN          "\033[96m"
#define PINK          "\033[95m"
#define BRIGHT_PINK   "\033[38;5;213m"
#define PURPLE        "\033[35m"
#define BRIGHT_PURPLE "\033[95m"
#define ORANGE        "\033[38;5;214m"
#define BROWN         "\033[38;5;94m"

// Backgrounds
#define BG_YELLOW  "\033[43m"
#define BG_GREEN   "\033[42m"
#define BG_BLUE    "\033[44m"
#define BG_RED     "\033[41m"
#define BG_CYAN    "\033[46m"
#define BG_MAGENTA "\033[45m"
#define BG_WHITE   "\033[47m"
#define BG_BLACK   "\033[40m"
#define BG_GRAY    "\033[100m"

// Effects
#define UNDERLINE "\033[4m"
#define BOLD      "\033[1m"
#define ITALIC    "\033[3m"
#define DIM       "\033[2m"
#define INVERT    "\033[7m"

// Reset
#define RESET "\033[0m"

void color(const char *cl);

#endif // COLOR_H
