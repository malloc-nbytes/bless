#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define err(msg)                                \
    do {                                        \
        fprintf(stderr, "[Error]: " msg "\n");  \
        exit(1);                                \
    } while (0)

#define err_wargs(msg, ...)                                     \
    do {                                                        \
        fprintf(stderr, "[Error]: " msg "\n", __VA_ARGS__);     \
        exit(1);                                                \
    } while (0)

#define da_append(arr, len, cap, ty, value)                       \
    do {                                                          \
        if ((len) >= (cap)) {                                     \
            (cap) = !(cap) ? 2 : (cap) * 2;                       \
            (arr) = (ty)realloc((arr), (cap) * sizeof((arr)[0])); \
        }                                                         \
        (arr)[(len)] = (value);                                   \
        (len) += 1;                                               \
    } while (0)

#define da_remove(arr, len, idx)                \
    do {                                        \
        if ((idx) < (len) - 1) {                \
            memmove(&(arr)[(idx)],              \
                    &(arr)[(idx) + 1],          \
                    ((len) - (idx) - 1) * sizeof((arr)[0])); \
        }                                       \
        (len)--;                                \
    } while (0)

#define dyn_arr(ty, name)                               \
    struct {                                            \
        ty *data;                                       \
        size_t len, cap;                                \
    } name = {0}; {                                     \
        (name).data = s_malloc(sizeof(ty));             \
        (name).cap = 1, (name).len = 0;                 \
    }

#define BIT_SET(bits, bit) ((bits) & (bit)) != 0

#define SAFE_PEEK(arr, i, el) ((arr)[i] && (arr)[i] == el)

int regex(const char *pattern, const char *s);
void *s_malloc(size_t b);
void out(const char *msg, int newline);
char get_char(void);
void clear_msg(void);
void reset_scrn(void);
void append_str(char **dest, size_t *size, const char *format, ...);

#endif // UTILS_H
