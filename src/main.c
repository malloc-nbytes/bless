#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

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

#define ENTER(ch)     (ch) == '\n'
#define BACKSPACE(ch) (ch) == 8 || (ch) == 127
#define ESCSEQ(ch)    (ch) == 27
#define CSI(ch)       (ch) == '['
#define TAB(ch)       (ch) == '\t'
#define UP_ARROW      'A'
#define DOWN_ARROW    'B'
#define RIGHT_ARROW   'C'
#define LEFT_ARROW    'D'

#define CTRL_N 0x0E // Scroll down
#define CTRL_D 0x04 // Page down
#define CTRL_L 0x0C // Refresh

#define da_append(arr, len, cap, ty, value)                       \
    do {                                                          \
        if ((len) >= (cap)) {                                     \
            (cap) = !(cap) ? 2 : (cap) * 2;                       \
            (arr) = (ty)realloc((arr), (cap) * sizeof((arr)[0])); \
        }                                                         \
        (arr)[(len)] = (value);                                   \
        (len) += 1;                                               \
    } while (0)

#define MAT_AT(m, c, i, j) \
    ((m)[(i) * (c) + (j)])

#define DEF_WIN_WIDTH 80
#define DEF_WIN_HEIGHT 24

static int g_win_width = DEF_WIN_WIDTH;
static int g_win_height = DEF_WIN_HEIGHT;
static struct termios g_old_termios;
static char *g_last_search = NULL;

typedef struct {
    char *data;
    size_t rows, cols;
} Matrix;

typedef struct {
    char *data;
    size_t len, cap;
} Dyn_Str;

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

    char *buffer = malloc(length + 1);
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

    Matrix matrix = {
        .data = malloc(rows * cols * sizeof(char)),
        .rows = rows,
        .cols = cols
    };

    if (!matrix.data) {
        perror("Failed to allocate matrix memory");
        exit(EXIT_FAILURE);
    }

    memset(matrix.data, ' ', rows * cols);

    size_t row = 0, col = 0;
    for (size_t i = 0; src[i]; ++i) {
        if (src[i] == '\n') {
            row++;
            col = 0;
        } else {
            matrix.data[row * cols + col] = src[i];
            col++;
        }
    }

    return matrix;
}

Dyn_Str get_user_input_in_mini_buffer(char *prompt, char *last_input) {
    assert(prompt);
    out(prompt, 0);

    Dyn_Str input = (Dyn_Str) {
        .data = malloc(1),
        .len = 0,
        .cap = 1,
    };

    if (last_input) {
        out(last_input, 0);
        for (size_t i = 0; last_input[i]; ++i)
            da_append(input.data, input.len, input.cap, char *, last_input[i]);
    }

    while (1) {
        char c = get_char();
        if (ENTER(c))
            break;
        if (BACKSPACE(c)) {
            out("\b \b", 0);
            input.data[input.len--] = '\0';
        } else {
            da_append(input.data, input.len, input.cap, char *, c);
        }
        putchar(c);
        fflush(stdout);
    }

    out("\r\033[K", 1);
    return input;
}

void help(void) {
    printf("Usage: fp <filepath>\n");
    exit(EXIT_FAILURE);
}

void dump_matrix(const Matrix *const matrix, size_t start_row, size_t end_row) {
    for (size_t i = start_row; i < end_row + start_row; ++i) {
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
size_t find_word_in_matrix(Matrix *matrix, size_t start_row, char *word, size_t word_len, int reverse) {
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

    size_t found = find_word_in_matrix(matrix, start_row, actual, actual_len, reverse);

    if (found) {
        *line = found;
        dump_matrix(matrix, *line, g_win_height);
    }
    else {
        reset_scrn();
        dump_matrix(matrix, *line, g_win_height);
        color(RED BOLD);
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
        color(RED BOLD);
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

int main(int argc, char **argv) {
    if (argc <= 1 || argc > 2)
        help();
    ++argv, --argc;

    atexit(cleanup);
    init_term();

    const char *fp = *argv;
    const char *src = file_to_cstr(fp);

    if (!src) {
        perror("src is NULL");
        exit(EXIT_FAILURE);
    }

    Matrix matrix = init_matrix(src);
    dump_matrix(&matrix, 0, g_win_height);

    size_t line = 0;
    while (1) {
        char c = get_char();
        if (ESCSEQ(c)) {
            int next0 = get_char();
            if (CSI(next0)) {
                int next1 = get_char();
                switch (next1) {
                case UP_ARROW:   handle_scroll_up(&matrix, &line);   break;
                case DOWN_ARROW: handle_scroll_down(&matrix, &line); break;
                }
            } else { // [ALT] key
                ;
            }
        }
        else if (c == 'n' || c == 'j') {
            handle_scroll_down(&matrix, &line);
        } else if (c == 'p' || c == 'k') {
            handle_scroll_up(&matrix, &line);
        } else if (c == 'g') {
            handle_jump_to_top(&matrix, &line);
        } else if (c == 'G') {
            handle_jump_to_bottom(&matrix, &line);
        } else if (c == '/') {
            handle_search(&matrix, &line, line, NULL, 0);
        } else if (c == 'N') {
            jump_to_last_searched_word(&matrix, &line, 0);
        } else if (c == 'P') {
            jump_to_last_searched_word(&matrix, &line, 1);
        } else if (c == 'q') {
            reset_scrn();
            break;
        }
    }

    return 0;
}
