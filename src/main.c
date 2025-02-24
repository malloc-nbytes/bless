#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdint.h>
#include <regex.h>

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

// Effects
#define UNDERLINE "\033[4m"
#define BOLD      "\033[1m"
#define ITALIC    "\033[3m"
#define DIM       "\033[2m"
#define INVERT    "\033[7m"

// Reset
#define RESET "\033[0m"

#define CTRL_N 14 // Scroll down
#define CTRL_D 4  // Page down
#define CTRL_U 21 // Page up
#define CTRL_L 12 // Refresh
#define CTRL_P 16 // Scroll up
#define CTRL_G 7  // Cancel
#define CTRL_V 22 // Scroll down
#define UP_ARROW      'A'
#define DOWN_ARROW    'B'
#define RIGHT_ARROW   'C'
#define LEFT_ARROW    'D'

#define FLAG_1HY_HELP   'h'
#define FLAG_1HY_ONCE   'o'
#define FLAG_1HY_LINES  'l'
#define FLAG_1HY_FILTER 'f'

#define FLAG_2HY_HELP   "--help"
#define FLAG_2HY_ONCE   "--once"
#define FLAG_2HY_LINES  "--lines"
#define FLAG_2HY_FILTER "--filter"

#define ENTER(ch)     (ch) == '\n'
#define BACKSPACE(ch) (ch) == 8 || (ch) == 127
#define ESCSEQ(ch)    (ch) == 27
#define CSI(ch)       (ch) == '['
#define TAB(ch)       (ch) == '\t'

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

#define BIT_SET(bits, bit) ((bits) & (bit)) != 0

#define SAFE_PEEK(arr, i, el) ((arr)[i] && (arr)[i] == el)

#define MAT_AT(m, c, i, j) \
    ((m)[(i) * (c) + (j)])

#define DEF_WIN_WIDTH 80
#define DEF_WIN_HEIGHT 24

static int g_win_width = DEF_WIN_WIDTH;
static int g_win_height = DEF_WIN_HEIGHT;
static struct termios g_old_termios;
static char *g_last_search = NULL;
static uint32_t g_flags = 0x0;
static char *g_filter_pattern = NULL;

typedef enum {
    FLAG_TYPE_HELP   = 1 << 0,
    FLAG_TYPE_ONCE   = 1 << 1,
    FLAG_TYPE_LINES  = 1 << 2,
    FLAG_TYPE_FILTER = 1 << 3,
} Flag_Type;

typedef enum {
    USER_INPUT_TYPE_CTRL,
    USER_INPUT_TYPE_ALT,
    USER_INPUT_TYPE_ARROW,
    USER_INPUT_TYPE_NORMAL,
    USER_INPUT_TYPE_UNKNOWN,
} User_Input_Type;

typedef struct {
    char *data;
    size_t rows, cols;
} Matrix;

typedef struct {
    char *data;
    size_t len, cap;
} Dyn_Str;

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

void color(const char *cl) {
    out(cl, 0);
}

char get_char(void) {
    char ch;
    read(STDIN_FILENO, &ch, 1);
    return ch;
}

User_Input_Type get_user_input(char *c) {
    assert(c);
    while (1) {
        *c = get_char();
        if (ESCSEQ(*c)) {
            int next0 = get_char();
            if (CSI(next0)) {
                int next1 = get_char();
                switch (next1) {
                case DOWN_ARROW:
                case UP_ARROW:
                    *c = next1;
                    return USER_INPUT_TYPE_ARROW;
                default:
                    return USER_INPUT_TYPE_UNKNOWN;
                }
            } else { // [ALT] key
                return USER_INPUT_TYPE_ALT;
            }
        }
        else if (*c == CTRL_N) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_P) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_G) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_D) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_U) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_V) return USER_INPUT_TYPE_CTRL;
        else return USER_INPUT_TYPE_NORMAL;
    }
    return USER_INPUT_TYPE_UNKNOWN;
}

void cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_termios);
}

void init_term(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
        g_win_width = w.ws_col, g_win_height = w.ws_row;
    else
        perror("ioctl failed");

    tcgetattr(STDIN_FILENO, &g_old_termios);
    struct termios raw = g_old_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

const char *file_to_cstr(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *buffer = (char *)s_malloc(length + 1);
    if (!buffer) {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    fclose(file);

    buffer[length] = '\0';

    return buffer;
}

Matrix init_matrix(const char *src) {
    size_t rows = 1, cols = 0, current_cols = 0;

    for (size_t i = 0; src[i]; ++i) {
        if (src[i] == '\n') {
            rows++;
            if (current_cols > cols)
                cols = current_cols;
            current_cols = 0;
        } else {
            current_cols++;
        }
    }
    if (current_cols > cols) cols = current_cols;

    Matrix matrix = (Matrix) {
        .data = (char *)s_malloc(rows * cols * sizeof(char)),
        .rows = rows,
        .cols = cols
    };

    if (!matrix.data) {
        perror("Failed to allocate matrix memory");
        exit(EXIT_FAILURE);
    }

    memset(matrix.data, ' ', rows * cols);

    size_t row = 0, col = 0;
    char buf[1024] = {0};
    size_t buf_len = 0;
    for (size_t i = 0; src[i]; ++i) {
        if (src[i] == '\n') {
            if (g_filter_pattern && !regex(g_filter_pattern, buf))
                goto not_match;
            for (size_t j = 0; j < buf_len; ++j)
                matrix.data[row * cols + j] = buf[j];
            row++;
        not_match:
            memset(buf, '\0', 1024);
            buf_len = 0;
        }
        else
            buf[buf_len++] = src[i];
    }

    matrix.rows = row;

    return matrix;
}

Dyn_Str get_user_input_in_mini_buffer(char *prompt, char *last_input) {
    assert(prompt);
    out(prompt, 0);

    Dyn_Str input = (Dyn_Str) {
        .data = (char *)s_malloc(1),
        .len = 0,
        .cap = 1,
    };

    if (last_input) {
        out(last_input, 0);
        for (size_t i = 0; last_input[i]; ++i)
            da_append(input.data, input.len, input.cap, char *, last_input[i]);
    }

    while (1) {
        char c;
        User_Input_Type ty = get_user_input(&c);
        switch (ty) {
        case USER_INPUT_TYPE_CTRL: {
            if (c == CTRL_G) {
                input.len = 0;
                goto ok;
            }
        } break;
        case USER_INPUT_TYPE_ALT:   break;
        case USER_INPUT_TYPE_ARROW: break;
        case USER_INPUT_TYPE_NORMAL: {
            if (ENTER(c)) goto ok;
            if (BACKSPACE(c)) {
                out("\b \b", 0);
                input.data[input.len--] = '\0';
            }
            else
                da_append(input.data, input.len, input.cap, char *, c);
        } break;
        case USER_INPUT_TYPE_UNKNOWN: break;
        default: break;
        }
        putchar(c);
        fflush(stdout);
    }

 ok:
    out("\r\033[K", 1);
    return input;
}

void help(void) {
    printf("Usage: bless <filepath>\n");
    exit(EXIT_FAILURE);
}

void dump_matrix(const Matrix *const matrix, size_t start_row, size_t end_row) {
    if (end_row > matrix->rows)
        end_row = matrix->rows;

    for (size_t i = start_row; i < end_row + start_row; ++i) {
        if (BIT_SET(g_flags, FLAG_TYPE_LINES))
            printf("%zu: ", i);
        for (size_t j = 0; j < matrix->cols; ++j)
            putchar(MAT_AT(matrix->data, matrix->cols, i, j));
        putchar('\n');
    }
}

void reset_scrn(void) {
    printf("\033[2J");
    printf("\033[H");
    fflush(stdout);
}

void handle_scroll_down(const Matrix *const matrix, size_t *const line) {
    if (*line + g_win_height < matrix->rows) {
        reset_scrn();
        dump_matrix(matrix, ++(*line), g_win_height);
    }
}

void handle_scroll_up(const Matrix *const matrix, size_t *const line) {
    if (*line > 0) {
        reset_scrn();
        dump_matrix(matrix, --(*line), g_win_height);
    }
}

void handle_jump_to_top(const Matrix *const matrix, size_t *const line) {
    reset_scrn();
    *line = 0;
    dump_matrix(matrix, *line, g_win_height);
}

void handle_jump_to_bottom(const Matrix *const matrix, size_t *const line) {
    reset_scrn();
    *line = matrix->rows - g_win_height;
    dump_matrix(matrix, *line, g_win_height);
}

// Returns the row in which the word was found.
int find_word_in_matrix(Matrix *matrix, size_t start_row, char *word, size_t word_len, int reverse) {
    size_t match = 0;
    int found = 0;

    if (!reverse) {
        for (size_t i = start_row; i < matrix->rows && !found; ++i) {
            for (size_t j = 0; j < matrix->cols && !found; ++j) {
                if (MAT_AT(matrix->data, matrix->cols, i, j) == word[match]) {
                    ++match;
                    if (match == word_len) {
                        found = i;
                        break;
                    }
                } else {
                    match = 0;
                }
            }
        }
    } else {
        for (size_t i = start_row + 1; i-- > 0 && !found; ) {
            for (size_t j = 0; j < matrix->cols && !found; ++j) {
                if (MAT_AT(matrix->data, matrix->cols, i, j) == word[match]) {
                    ++match;
                    if (match == word_len) {
                        found = i;
                        break;
                    }
                } else {
                    match = 0;
                }
            }
        }
    }

    return found ? found - 1 : 0;
}

void handle_search(Matrix *matrix, size_t *line, size_t start_row, char *jump_to_next/*optional*/, int reverse) {
    char *actual = NULL;
    size_t actual_len = 0;

    if (!jump_to_next) {
        Dyn_Str input = get_user_input_in_mini_buffer("/", g_last_search);
        actual = input.data;
        actual_len = input.len;
    }
    else {
        actual = jump_to_next;
        actual_len = strlen(actual);
    }

    if (actual_len == 0)
        return;

    size_t found = find_word_in_matrix(matrix, start_row, actual, actual_len, reverse);

    if (found) {
        *line = found;
        dump_matrix(matrix, *line, g_win_height);
    }
    else {
        reset_scrn();
        dump_matrix(matrix, *line, g_win_height-1);
        color(RED BOLD UNDERLINE);
        out("--- SEARCH NOT FOUND ---", 1);
        color(RESET);
    }

    if (!jump_to_next) {
        if (g_last_search)
            free(g_last_search);
        g_last_search = actual;
    }
}

void jump_to_last_searched_word(Matrix *matrix, size_t *line, int reverse) {
    if (!g_last_search) {
        reset_scrn();
        dump_matrix(matrix, *line, g_win_height-1);
        color(RED BOLD UNDERLINE);
        out("--- NO PREVIOUS SEARCH ---", 1);
        color(RESET);
        return;
    }
    if (!reverse) {
        handle_search(matrix, line, *line+2, g_last_search, 0);
    } else {
        handle_search(matrix, line, *line, g_last_search, 1);
    }
}

void handle_page_up(Matrix *matrix, size_t *line) {
    if (*line > 0) {
        if (*line < g_win_height / 2)
            *line = 0;
        else
            *line -= g_win_height / 2;
        dump_matrix(matrix, *line, g_win_height);
    }
}

void handle_page_down(Matrix *matrix, size_t *line) {
    size_t max_start = matrix->rows > g_win_height
        ? matrix->rows - g_win_height
        : 0;

    if (*line < max_start) {
        *line += g_win_height / 2;
        if (*line > max_start)
            *line = max_start;
        dump_matrix(matrix, *line, g_win_height);
    }
}

void redraw_matrix(Matrix *matrix, size_t line) {
    reset_scrn();
    dump_matrix(matrix, line, g_win_height);
}

char *eat(int *argc, char ***argv) {
    if (!(*argc))
        return NULL;
    (*argc)--;
    return *(*argv)++;
}

void handle_filter_flag(int *argc, char ***argv) {
    g_filter_pattern = eat(argc, argv);
}

void handle_2hy_flag(const char *arg, int *argc, char ***argv) {
    if (!strcmp(arg, FLAG_2HY_HELP))
        help();
    else if (!strcmp(arg, FLAG_2HY_ONCE))
        g_flags |= FLAG_TYPE_ONCE;
    else if (!strcmp(arg, FLAG_2HY_LINES))
        g_flags |= FLAG_TYPE_LINES;
    else if (!strcmp(arg, FLAG_2HY_FILTER)) {
        g_flags |= FLAG_TYPE_FILTER;
        handle_filter_flag(argc, argv);
    }
    else
        err_wargs("Unknown option: `%s`", arg);
}

void handle_1hy_flag(const char *arg, int *argc, char ***argv) {
    const char *it = arg+1;
    while (it && *it != ' ' && *it != '\0') {
        if (*it == FLAG_1HY_HELP)
            help();
        else if (*it == FLAG_1HY_ONCE)
            g_flags |= FLAG_TYPE_ONCE;
        else if (*it == FLAG_1HY_LINES)
            g_flags |= FLAG_TYPE_LINES;
        else if (*it == FLAG_1HY_FILTER) {
            g_flags |= FLAG_TYPE_FILTER;
            handle_filter_flag(argc, argv);
        }
        else
            err_wargs("Unknown option: `%c`", *it);
        ++it;
    }
}

int main(int argc, char **argv) {
    if (argc <= 1)
        help();
    ++argv, --argc;

    struct {
        char **actual;
        size_t len, cap;
    } paths = {0}; {
        paths.actual = (char **)s_malloc(sizeof(char *));
        paths.len = 0, paths.cap = 1;
    };

    char *arg = NULL;
    while ((arg = eat(&argc, &argv)) != NULL) {
        if (arg[0] == '-' && SAFE_PEEK(arg, 1, '-'))
            handle_2hy_flag(arg, &argc, &argv);
        else if (arg[0] == '-' && arg[1])
            handle_1hy_flag(arg, &argc, &argv);
        else
            da_append(paths.actual, paths.len, paths.cap, char **, arg);
    }

    atexit(cleanup);
    init_term();

    for (size_t i = 0; i < paths.len; ++i) {
        const char *fp = paths.actual[i];
        const char *src = file_to_cstr(fp);

        if (!src) {
            perror("src is NULL");
            exit(EXIT_FAILURE);
        }

        Matrix matrix = init_matrix(src);

        if (BIT_SET(g_flags, FLAG_TYPE_ONCE)) {
            dump_matrix(&matrix, 0, matrix.rows);
            continue;
        }

        dump_matrix(&matrix, 0, g_win_height);

        size_t line = 0;
        while (1) {
            char c;
            User_Input_Type ty = get_user_input(&c);
            switch (ty) {
            case USER_INPUT_TYPE_CTRL: {
                if (c == CTRL_N)      handle_scroll_down(&matrix, &line);
                else if (c == CTRL_P) handle_scroll_up(&matrix, &line);
                else if (c == CTRL_D) handle_page_down(&matrix, &line);
                else if (c == CTRL_V) handle_page_down(&matrix, &line);
                else if (c == CTRL_U) handle_page_up(&matrix, &line);
            } break;
            case USER_INPUT_TYPE_ALT: {} break;
            case USER_INPUT_TYPE_ARROW: {
                if (c == UP_ARROW)        handle_scroll_up(&matrix, &line);
                else if (c == DOWN_ARROW) handle_scroll_down(&matrix, &line);
            } break;
            case USER_INPUT_TYPE_NORMAL: {
                if (c == 'k')      handle_scroll_up(&matrix, &line);
                else if (c == 'j') handle_scroll_down(&matrix, &line);
                else if (c == 'g') handle_jump_to_top(&matrix, &line);
                else if (c == 'G') handle_jump_to_bottom(&matrix, &line);
                else if (c == '/') handle_search(&matrix, &line, line, NULL, 0);
                else if (c == 'n') jump_to_last_searched_word(&matrix, &line, 0);
                else if (c == 'N') jump_to_last_searched_word(&matrix, &line, 1);
                else if (c == 'q') goto done;
                else if (c == 'Q') {
                    reset_scrn();
                    free(matrix.data);
                    goto end;
                }
                else redraw_matrix(&matrix, line);
            } break;
            case USER_INPUT_TYPE_UNKNOWN: {} break;
            default: {} break;
            }
        }

    done:
        reset_scrn();
        free(matrix.data);
    }

 end:
    return 0;
}
