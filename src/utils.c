#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <regex.h>

#include "utils.h"

int regex(const char *pattern, const char *s) {
    regex_t regex;
    int reti;

    reti = regcomp(&regex, pattern, 0);
    if (reti) {
        perror("regex");
        return 0;
    }

    reti = regexec(&regex, s, 0, NULL, 0);

    regfree(&regex);

    if (!reti) return 1;
    else       return 0;
}

void *s_malloc(size_t b) {
    void *p = malloc(b);
    if (!p) err_wargs("could not allocate %zu bytes", b);
    return p;
}

void out(const char *msg, int newline) {
    printf("%s", msg);
    if (newline)
        putchar('\n');
    fflush(stdout);
}

char get_char(void) {
    char ch;
    read(STDIN_FILENO, &ch, 1);
    return ch;
}

void clear_msg(void) {
    out("\r\033[K", 0);
}

void reset_scrn(void) {
    printf("\033[2J");
    printf("\033[H");
    fflush(stdout);
}

void append_str(char **dest, size_t *size, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char temp[1024];
    int len = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    if (len < 0) return;

    size_t new_size = *size + len + 1;
    char *new_str = realloc(*dest, new_size);
    if (!new_str) return;

    strcpy(new_str + *size, temp);
    *dest = new_str;
    *size += len;
}
