#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdint.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <ctype.h>

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

#define CTRL_N 14 // Scroll down
#define CTRL_D 4  // Page down
#define CTRL_U 21 // Page up
#define CTRL_L 12 // Refresh
#define CTRL_P 16 // Scroll up
#define CTRL_G 7  // Cancel
#define CTRL_V 22 // Scroll down
#define CTRL_W 23 // Save buffer
#define CTRL_O 15 // Open buffer
//#define CTRL_I 9  // Open editor

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

#define BUFFERS_LIM 256

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

static char *g_ob_fp = "bless-open-buffer";
static char *g_iu_fp = "bless-usage";
static char *g_usage = "Bless internal usage buffer:\n\n"
"__________.__                        \n"
"\\______   \\  |   ____   ______ ______\n"
" |    |  _/  | _/ __ \\ /  ___//  ___/\n"
" |    |   \\  |_\\  ___/ \\___ \\ \\___ \\ \n"
" |______  /____/\\___  >____  >____  >\n"
"        \\/          \\/     \\/     \\/ \n\n"
    "Quick navigation:\n"
    "C-n or j for scroll down\n"
    "C-p or k for scroll up\n\n"
    "Search\n"
    "    /           Enable search mode\n"
    "    n           Next occurrence\n"
    "    N           Previous occurrence\n"
    "    C-g         Cancel search\n"
    "Vim Keybindings:\n"
    "    j           Scroll down\n"
    "    k           Scroll up\n"
    "    h           Left buffer\n"
    "    l           Right buffer\n"
    "    z           Put the top line in the center of the screen\n"
    "    C-d         Page down\n"
    "    C-u         Page up\n"
    "Emacs Keybindings\n"
    "    C-n         Scroll down\n"
    "    C-p         Scroll up\n"
    "    C-l         Put the top line in the center of the screen\n"
    "    C-v         Page down\n"
    "    M-v         Page up\n"
    "Agnostic Keybindings\n"
    "    ?           Open this usage buffer\n"
    "    :           Special input mode\n"
    "    L           Refresh buffer\n"
    "    O           Open file in place\n"
    "    q           Quit buffer\n"
    "    d           Quit buffer\n"
    "    Q           Quit all buffers\n"
    "    D           Quit all buffers\n"
    "    J           Left buffer\n"
    "    K           Right buffer\n"
    "    C-w         Save a buffer\n"
    "    C-o         Open a saved buffer\n"
    "    I           Open the current line in Vim\n"
    "    [UP]        Scroll up\n"
    "    [DOWN]      Scroll down\n"
    "    [LEFT]      Left buffer\n"
    "    [RIGHT]     Right buffer\n"
    "";
static int            g_win_width      = DEF_WIN_WIDTH;
static int            g_win_height     = DEF_WIN_HEIGHT;
static char          *g_last_search    = NULL;
static uint32_t       g_flags          = 0x0;
static char          *g_filter_pattern = NULL;
static struct termios g_old_termios;

static struct {
    char **paths;
    size_t len, cap;
    size_t *last_saved_lines;
    size_t lsl_len, lsl_cap;
} g_saved_buffers = {0};

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
    char *filepath;
} Matrix;

void init_config_file(void) {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[Warning]: HOME environment variable not set.\n");
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/.bless", home);

    if (access(path, F_OK) == 0) {
        return; // File exists, do nothing
    }

    FILE *file = fopen(path, "w");
    if (!file) {
        fprintf(stderr, "[Warning] Could not create config file: %s\n", path);
        return;
    }

    fclose(file);
}

void remove_entry_from_config_file(int idx) {
    const char *home_dir = getenv("HOME");
    if (!home_dir) {
        fprintf(stderr, "HOME environment variable is not set.\n");
        return;
    }

    size_t config_path_len = strlen(home_dir) + strlen("/.bless") + 1;
    char *config_file_path = malloc(config_path_len);
    if (!config_file_path) {
        perror("Failed to allocate memory for config file path");
        return;
    }
    snprintf(config_file_path, config_path_len, "%s/.bless", home_dir);

    FILE *file = fopen(config_file_path, "r+");
    if (!file) {
        perror("Failed to open config file");
        free(config_file_path);
        return;
    }

    char **lines = NULL;
    size_t lines_count = 0;
    size_t lines_capacity = 10;

    lines = malloc(sizeof(char *) * lines_capacity);
    if (!lines) {
        perror("Failed to allocate memory for lines");
        fclose(file);
        free(config_file_path);
        return;
    }

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        // Skip leading newlines in the file
        if (strlen(line) == 1 && line[0] == '\n') {
            continue; // Skip empty lines (just newlines)
        }

        da_append(lines, lines_count, lines_capacity, char **, strdup(line));
    }
    free(line);

    // Remove the entry at the specified index
    if (idx < 0 || idx >= lines_count) {
        fprintf(stderr, "Invalid index\n");
        free(lines);
        fclose(file);
        free(config_file_path);
        return;
    }

    free(lines[idx]);

    // Shift the remaining lines down
    for (size_t i = idx; i < lines_count - 1; ++i)
        lines[i] = lines[i + 1];
    lines_count--;

    // Write the updated content back to the file
    freopen(config_file_path, "w", file);
    if (!file) {
        perror("Failed to reopen config file for writing");
        free(lines);
        free(config_file_path);
        return;
    }

    for (size_t i = 0; i < lines_count; ++i) {
        fputs(lines[i], file);
        free(lines[i]);
    }

    free(lines);
    fclose(file);
    free(config_file_path);
}

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
                case RIGHT_ARROW:
                case LEFT_ARROW:
                case UP_ARROW:
                    *c = next1;
                    return USER_INPUT_TYPE_ARROW;
                default:
                    return USER_INPUT_TYPE_UNKNOWN;
                }
            } else { // [ALT] key
                *c = next0;
                return USER_INPUT_TYPE_ALT;
            }
        }
        else if (*c == CTRL_N) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_P) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_G) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_D) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_U) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_V) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_W) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_O) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_L) return USER_INPUT_TYPE_CTRL;
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
        g_win_width = w.ws_col-1, g_win_height = w.ws_row-1;
    else
        perror("ioctl failed");

    tcgetattr(STDIN_FILENO, &g_old_termios);
    struct termios raw = g_old_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

char *expand_tilde(const char *path) {
    if (path[0] != '~') {
        return strdup(path); // No tilde, return a copy of the original string
    }

    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        return NULL;
    }

    size_t home_len = strlen(home);
    size_t path_len = strlen(path);

    char *expanded_path = malloc(home_len + path_len); // +1 is already included since `~` is replaced
    if (!expanded_path) {
        perror("Failed to allocate memory");
        return NULL;
    }

    strcpy(expanded_path, home);
    strcat(expanded_path, path + 1); // Skip the `~`

    return expanded_path;
}

const char *file_to_cstr(const char *filename) {
    char *expanded_path = expand_tilde(filename);
    if (!expanded_path) return NULL;

    FILE *file = fopen(expanded_path, "rb");
    free(expanded_path); // Don't forget to free after use!

    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(length + 1);
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

const char *get_line_from_cstr(const char *fp, size_t lineno) {
    FILE *file = fopen(fp, "rb");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    char line[512];
    size_t i = 0;
    while (fgets(line, sizeof(line), file)) {
        if (i == lineno) return strdup(line);
        ++i;
    }
    return NULL;
}

Matrix init_matrix(const char *src, char *filepath) {
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
        .cols = cols,
        .filepath = filepath,
    };

    if (!matrix.data) {
        perror("Failed to allocate matrix memory");
        exit(EXIT_FAILURE);
    }

    memset(matrix.data, ' ', rows * cols);

    size_t row = 0, col = 0;

    struct {
        char *chars;
        size_t len, cap;
    } buf = {0}; {
        buf.chars = (char *)s_malloc(80);
        buf.cap = 80, buf.len = 0;
        memset(buf.chars, '\0', buf.cap);
    }

    for (size_t i = 0; src[i]; ++i) {
        if (src[i] == '\n') {
            if (g_filter_pattern && !regex(g_filter_pattern, buf.chars))
                goto not_match;
            for (size_t j = 0; j < buf.len; ++j)
                matrix.data[row * cols + j] = buf.chars[j];
            row++;
        not_match:
            memset(buf.chars, '\0', buf.len);
            buf.len = 0;
        }
        else
            da_append(buf.chars, buf.len, buf.cap, char *, src[i]);
    }

    matrix.rows = row;

    free(buf.chars);

    return matrix;
}

void clear_msg(void) {
    out("\r\033[K", 0);
}

// Caller must free()
void parse_config_file(char ***paths,
                       size_t **lines,
                       char ***names,
                       size_t *info_len) {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[Warning]: HOME environment variable not set.\n");
        return;
    }

    char bless[512];
    snprintf(bless, sizeof(bless), "%s/.bless", home);

    FILE *file = fopen(bless, "rb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            // Remove newline
            line[len - 1] = '\0';

        char *path = NULL;
        char *line_str = NULL;
        char *name = NULL;

        path = strtok(line, ":");      // First part: file path
        line_str = strtok(NULL, ":");  // Second part: line number
        name = strtok(NULL, ":");      // Third part: name

        if (path && line_str && name) {
            *paths = realloc(*paths, sizeof(char*) * (*info_len + 1));
            *names = realloc(*names, sizeof(char*) * (*info_len + 1));
            *lines = realloc(*lines, sizeof(size_t) * (*info_len + 1));

            (*paths)[*info_len] = strdup(path);
            (*names)[*info_len] = strdup(name);
            (*lines)[*info_len] = (size_t)atoi(line_str);

            (*info_len)++;
        }
    }

    fclose(file);
}

char *get_user_input_in_mini_buffer(char *prompt, char *last_input) {
    assert(prompt);

    color(YELLOW BOLD UNDERLINE);
    for (int i = 0; prompt[i]; ++i) {
        if (prompt[i] == ' ' && !prompt[i+1])
            color(RESET);
        putchar(prompt[i]);
    }
    color(RESET);

    const size_t
        input_lim = 256,
        prompt_len = strlen(prompt);

    char *input = (char *)s_malloc(input_lim);
    (void)memset(input, '\0', input_lim);
    size_t input_len = 0;

    if (last_input) {
        out(last_input, 0);
        size_t last_len = strlen(last_input);
        memcpy(input, last_input, last_len);
        input_len = last_len;
    }

    while (1) {
        char c;
        User_Input_Type ty = get_user_input(&c);
        switch (ty) {
        case USER_INPUT_TYPE_CTRL: {
            if (c == CTRL_G) {
                input = NULL;
                goto ok;
            }
        } break;
        case USER_INPUT_TYPE_ALT:   break;
        case USER_INPUT_TYPE_ARROW: break;
        case USER_INPUT_TYPE_NORMAL: {
            if (ENTER(c)) goto ok;
            if (BACKSPACE(c)) {
                if (input_len > 0) {
                    out("\b \b", 0);
                    input[--input_len] = '\0';
                }
            }
            else {
                if (input_len >= input_lim)
                    err_wargs("input length must be < %zu", input_lim);
                input[input_len++] = c;
            }
        } break;
        case USER_INPUT_TYPE_UNKNOWN: break;
        default: break;
        }
        putchar(c);
        fflush(stdout);
    }

 ok:
    clear_msg();
    return input;
}

void help(void) {
    printf("(MIT License) Copyright (c) 2025 malloc-nbytes\n\n");

    printf("Usage: bless [filepath...] [options...]\n");
    printf("Options:\n");
    printf("  %s,   -%c    Print this message\n", FLAG_2HY_HELP, FLAG_1HY_HELP);
    printf("  %s,   -%c    Just print the files out the files\n", FLAG_2HY_ONCE, FLAG_1HY_ONCE);
    printf("  %s,  -%c    Show Bless line numbers (not file line numbers)\n", FLAG_2HY_LINES, FLAG_1HY_LINES);
    printf("  %s, -%c    Filter using regex\n", FLAG_2HY_FILTER, FLAG_1HY_FILTER);
    exit(EXIT_FAILURE);
}

void dump_matrix(const Matrix *const matrix, size_t start_row, size_t end_row) {
    /* if (end_row > matrix->rows) */
    /*     end_row = matrix->rows; */

    for (size_t i = start_row; i < end_row + start_row; ++i) {
        if (BIT_SET(g_flags, FLAG_TYPE_LINES) && i < matrix->rows)
            printf("%zu: ", i+1);
        for (size_t j = 0; j < matrix->cols; ++j) {
            if (i >= matrix->rows)
                putchar(' ');
            else
                putchar(MAT_AT(matrix->data, matrix->cols, i, j));
        }
        putchar('\n');
    }
}

void reset_scrn(void) {
    printf("\033[2J");
    printf("\033[H");
    fflush(stdout);
}

#define err_msg_wmatrix_wargs(matrix, line, msg, ...)   \
    do {                                                \
        reset_scrn();                                   \
        dump_matrix(matrix, line, g_win_height-1);      \
        color(RED BOLD UNDERLINE);                      \
        printf(msg "\n", __VA_ARGS__);                  \
        fflush(stdout);                                 \
        color(RESET);                                   \
    } while (0)

#define err_msg_wmatrix(matrix, line, msg)              \
    do {                                                \
        reset_scrn();                                   \
        dump_matrix(matrix, line, g_win_height-1);      \
        color(RED BOLD UNDERLINE);                      \
        printf(msg "\n");                               \
        fflush(stdout);                                 \
        color(RESET);                                   \
    } while (0)

void handle_scroll_down(const Matrix *const matrix, size_t *const line) {
    // Scrolling down does not need bounds checking
    // because dump_matrix will fill out-of-bounds space
    // with empty spaces.
    reset_scrn();
    dump_matrix(matrix, ++(*line), g_win_height);
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

    if (!jump_to_next)
        actual = get_user_input_in_mini_buffer("[Search]: ", g_last_search);
    else
        actual = jump_to_next;

    if (!actual) return;

    actual_len = strlen(actual);

    if (actual_len == 0)
        return;

    size_t found = find_word_in_matrix(matrix, start_row, actual, actual_len, reverse);

    if (found) {
        *line = found+1;
        dump_matrix(matrix, *line, g_win_height);
    }
    else {
        err_msg_wmatrix_wargs(matrix, *line, "[SEARCH NOT FOUND: %s]", actual);
        return;
    }

    if (!jump_to_next) {
        if (g_last_search)
            free(g_last_search);
        g_last_search = actual;
    }
}

void jump_to_last_searched_word(Matrix *matrix, size_t *line, int reverse) {
    if (!g_last_search) {
        err_msg_wmatrix(matrix, *line, "[NO PREVIOUS SEARCH]");
        return;
    }
    if (!reverse) handle_search(matrix, line, *line+1, g_last_search, 0);
    else          handle_search(matrix, line, *line-1, g_last_search, 1);
}

void handle_page_up(Matrix *matrix, size_t *line) {
    if (*line > 0) {
        if (*line < g_win_height / 2) *line = 0;
        else                          *line -= g_win_height / 2;
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

void save_buffer(Matrix *matrix, size_t line) {
    char *name = get_user_input_in_mini_buffer("Save as: ", NULL);
    if (!name) return;

    // Check if filepath is absolute or relative
    char fullpath[PATH_MAX];
    if (matrix->filepath[0] == '/') {
        // Absolute path
        snprintf(fullpath, sizeof(fullpath), "%s", matrix->filepath);
    } else {
        // Relative path
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("Error getting current directory");
            return;
        }
        snprintf(fullpath, sizeof(fullpath), "%s/%s", cwd, matrix->filepath);
    }

    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[Warning]: HOME environment variable not set.\n");
        return;
    }

    char bless[512];
    snprintf(bless, sizeof(bless), "%s/.bless", home);

    FILE *file = fopen(bless, "a");
    if (!file) {
        perror("Error opening file");
        return;
    }

    char text[512];
    snprintf(text, sizeof(text), "%s:%zu:%s\n", fullpath, line, name);

    if (fputs(text, file) == EOF) {
        perror("Error writing to file");
    }

    fclose(file);
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

void display_tabs(const Matrix *const matrix,
                  char **paths,
                  size_t paths_len,
                  size_t *last_viewed_lines,
                  size_t line) {
    color(BOLD UNDERLINE);
    printf("[");
    for (size_t i = 0; i < paths_len; ++i) {
        if (!strcmp(paths[i], matrix->filepath)) {
            color(BG_GREEN BLACK);
            printf(" %s:%zu ", paths[i], line);
            color(RESET);
        }
        else {
            color(RESET BOLD UNDERLINE);
            printf(" %s:%zu ", paths[i], last_viewed_lines[i]);
        }
    }
    printf("]");
    color(RESET);
    fflush(stdout);

}

void launch_editor(Matrix *matrix, size_t line) {
    if (!strcmp(matrix->filepath, g_iu_fp) || !strcmp(matrix->filepath, g_ob_fp)) {
        err_msg_wmatrix_wargs(matrix, line, "Cannot edit buffer `%s` as it is internal", matrix->filepath);
        return;
    }

    if (!matrix || !matrix->filepath) {
        fprintf(stderr, "Error: Invalid matrix or filepath\n");
        return;
    }

    pid_t pid = fork();

    if (pid == 0) { // Child process
        char line_arg[32];
        snprintf(line_arg, sizeof(line_arg), "+%zu", line+1);

        execlp("vim", "vim", line_arg, matrix->filepath, NULL);
        perror("execlp failed"); // If execlp fails
        _exit(1);
    } else if (pid > 0) { // Parent process
        wait(NULL); // Wait for Vim to exit
    } else {
        perror("fork failed");
    }

    free(matrix->data);
    *matrix = init_matrix(file_to_cstr(matrix->filepath), matrix->filepath);
    reset_scrn();
    dump_matrix(matrix, line, g_win_height);
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

char *saved_buffer_contents_create() {
    char *output = calloc(1, 1);
    size_t output_size = 0;

    append_str(&output, &output_size, "\033[1;4m=== Saved Buffers ===\033[0m\n");
    append_str(&output, &output_size, "Use :<number> to select a buffer\n");
    append_str(&output, &output_size, "Use :r<number> (no space) to remove a bookmark\n");
    append_str(&output, &output_size, "This buffer will close upon selection\n\n");
    append_str(&output, &output_size, "You can populate this buffer by opening a file\n");
    append_str(&output, &output_size, "in Bless and doing C-w or by opening one directly\n");
    append_str(&output, &output_size, "with `O` and providing a path then doing C-w.\n\n");

    char **info_paths = malloc(sizeof(char *) * 10);
    size_t *info_lines = malloc(sizeof(size_t) * 10);
    char **info_names = malloc(sizeof(char *) * 10);
    size_t info_len = 0;

    parse_config_file(&info_paths, &info_lines, &info_names, &info_len);

    // Find max column widths
    size_t max_name_len = 0, max_path_len = 0;
    for (size_t i = 0; i < info_len; ++i) {
        size_t name_len = strlen(info_names[i]);
        size_t path_len = strlen(info_paths[i]);
        if (name_len > max_name_len) max_name_len = name_len;
        if (path_len > max_path_len) max_path_len = path_len;
    }

    size_t adjusted_name_len = max_name_len + 2;

    append_str(&output, &output_size, "%-4s %-*s %-*s %-*s\n", "Index", adjusted_name_len, "Name", max_path_len, "Path", 10, "Line");
    append_str(&output, &output_size, "----------------------------------------------------------------\n");

    for (size_t i = 0; i < info_len; ++i) {
        char *path = info_paths[i];
        const size_t line = info_lines[i];
        const char *name = info_names[i];

        append_str(&output, &output_size, "\033[1m%-4zu [%-*s] %-*s %-*zu\033[0m\n", i, adjusted_name_len, name, max_path_len, path, 10, line);

        const char *preview = get_line_from_cstr(path, line);
        if (preview) {
            while (*preview && *preview == ' ') ++preview;
            append_str(&output, &output_size, "└────── %s\n", preview);
        } else {
            append_str(&output, &output_size, "[no preview available]\n");
        }

        if (i >= g_saved_buffers.len) {
            da_append(g_saved_buffers.paths, g_saved_buffers.len, g_saved_buffers.cap, char **, path);
            da_append(g_saved_buffers.last_saved_lines, g_saved_buffers.lsl_len, g_saved_buffers.lsl_cap, size_t *, line);
        }
    }

    append_str(&output, &output_size, "\n");

    return output;
}

void center_scrn(const Matrix *const matrix, size_t *line) {
    /* if (!matrix || !line) return; // Guard against null pointers */
    /* if (matrix->rows == 0) return; // No rows in the matrix */

    /* if (*line < g_win_height / 2) { */
    /*     *line = 0; // Prevent underflow */
    /* } else if (*line > matrix->rows - 1) { */
    /*     *line = matrix->rows - 1; // Prevent out-of-bounds access */
    /* } else { */
    /*     *line = *line - g_win_height / 2; */
    /* } */

    /* reset_scrn(); */
    /* dump_matrix(matrix, *line, g_win_height); */
}

int main(int argc, char **argv) {
    g_saved_buffers.paths = s_malloc(sizeof(char *));
    g_saved_buffers.len = 0, g_saved_buffers.cap = 1;
    g_saved_buffers.last_saved_lines = s_malloc(sizeof(size_t));
    g_saved_buffers.lsl_len = 0;
    g_saved_buffers.lsl_cap = 1;

    init_config_file();

    ++argv, --argc;

    struct {
        char **actual;
        size_t len, cap;
    } paths = {0};
    {
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

    struct {
        Matrix matrices[BUFFERS_LIM];
        size_t len;
        size_t last_viewed_lines[BUFFERS_LIM];
    } buffers = {0}; { buffers.len = 0; }

    if (paths.len >= BUFFERS_LIM)
        err_wargs("Only %d files are supported", BUFFERS_LIM);

    for (size_t i = 0; i < paths.len; ++i) {
        const char *fp = paths.actual[i];
        const char *src = file_to_cstr(fp);

        if (!src) {
            perror("src is NULL");
            exit(EXIT_FAILURE);
        }

        Matrix matrix = init_matrix(src, paths.actual[i]);
        buffers.matrices[buffers.len] = matrix;
        buffers.last_viewed_lines[buffers.len++] = 0;
    }

    if (buffers.len == 0) {
        Matrix usage_matrix = init_matrix(g_usage, g_iu_fp);
        buffers.matrices[buffers.len] = usage_matrix;
        buffers.last_viewed_lines[buffers.len++] = 0;
        paths.actual[paths.len++] = g_iu_fp;
    }

    int b_idx = 0;
    while (1) {
        Matrix *matrix = &buffers.matrices[b_idx];

        if (BIT_SET(g_flags, FLAG_TYPE_ONCE)) {
            if (b_idx >= buffers.len)
                break;
            dump_matrix(matrix, 0, matrix->rows);
            ++b_idx;
            continue;
        }

        size_t line = buffers.last_viewed_lines[b_idx];
        reset_scrn();
        dump_matrix(matrix, line, g_win_height);

        display_tabs(matrix, paths.actual, paths.len, buffers.last_viewed_lines, line);

        while (1) {
            char c;
            User_Input_Type ty = get_user_input(&c);

            clear_msg();

            switch (ty) {
            case USER_INPUT_TYPE_CTRL: {
                if (c == CTRL_N)      handle_scroll_down(matrix, &line);
                else if (c == CTRL_P) handle_scroll_up(matrix, &line);
                else if (c == CTRL_D) handle_page_down(matrix, &line);
                else if (c == CTRL_V) handle_page_down(matrix, &line);
                else if (c == CTRL_U) handle_page_up(matrix, &line);
                else if (c == CTRL_W) save_buffer(matrix, line);
                else if (c == CTRL_L) handle_page_up(matrix, &line);
                else if (c == CTRL_O) {
                    char *saved_buffer_contents = saved_buffer_contents_create();
                    Matrix open_buffer_matrix = init_matrix(saved_buffer_contents, g_ob_fp);

                    buffers.matrices[buffers.len] = open_buffer_matrix;
                    buffers.last_viewed_lines[buffers.len++] = 0;
                    b_idx = buffers.len-1;
                    paths.actual[paths.len++] = g_ob_fp;

                    goto switch_buffer;
                }
            } break;
            case USER_INPUT_TYPE_ALT: {
                if (c == 'v') handle_page_up(matrix, &line);
            } break;
            case USER_INPUT_TYPE_ARROW: {
                if (c == UP_ARROW)        handle_scroll_up(matrix, &line);
                else if (c == DOWN_ARROW) handle_scroll_down(matrix, &line);
                else if (c == RIGHT_ARROW && b_idx < buffers.len-1) {
                    buffers.last_viewed_lines[b_idx++] = line;
                    goto switch_buffer;
                }
                else if (c == LEFT_ARROW && b_idx > 0) {
                    buffers.last_viewed_lines[b_idx--] = line;
                    goto switch_buffer;
                }
            } break;
            case USER_INPUT_TYPE_NORMAL: {
                if (c == 'k')      handle_scroll_up(matrix, &line);
                else if (c == 'j') handle_scroll_down(matrix, &line);
                else if (c == 'g') handle_jump_to_top(matrix, &line);
                else if (c == 'G') handle_jump_to_bottom(matrix, &line);
                else if (c == '/') handle_search(matrix, &line, line, NULL, 0);
                else if (c == ':') {
                    char *inp = get_user_input_in_mini_buffer(": ", NULL);
                    int is_open_buffer = !strcmp(matrix->filepath, g_ob_fp);
                    if (is_open_buffer && inp && isdigit(inp[0])) {
                        int idx = atoi(inp);
                        Matrix selected_matrix = init_matrix(file_to_cstr(g_saved_buffers.paths[idx]), g_saved_buffers.paths[idx]);
                        buffers.matrices[buffers.len] = selected_matrix;
                        buffers.last_viewed_lines[buffers.len++] = g_saved_buffers.last_saved_lines[idx];
                        paths.actual[paths.len++] = selected_matrix.filepath;
                        goto delete_buffer;
                    } else if (is_open_buffer && inp && inp[0] == 'r') {
                        int idx = atoi(inp+1);
                        remove_entry_from_config_file(idx);
                        free(matrix->data);
                        char *saved_buffer_contents = saved_buffer_contents_create();
                        *matrix = init_matrix(saved_buffer_contents, g_ob_fp);
                        goto switch_buffer;
                    } else if (!inp) {
                        break;
                    }
                    else {
                        assert(0 && "unimplemented");
                    }
                }
                else if (c == 'n') jump_to_last_searched_word(matrix, &line, 0);
                else if (c == 'N') jump_to_last_searched_word(matrix, &line, 1);
                else if (c == 'I') launch_editor(matrix, line);
                else if (c == 'L') redraw_matrix(matrix, line);
                else if (c == 'z') handle_page_up(matrix, &line);
                else if (c == 'O') {
                    char *new_filepath = get_user_input_in_mini_buffer("Path: ", NULL);
                    if (!new_filepath)
                        break;
                    new_filepath = expand_tilde(new_filepath);
                    Matrix new_matrix = init_matrix(file_to_cstr(new_filepath), new_filepath);
                    buffers.matrices[buffers.len] = new_matrix;
                    buffers.last_viewed_lines[buffers.len++] = 0;
                    b_idx = buffers.len-1;
                    paths.actual[paths.len++] = new_filepath;
                    goto switch_buffer;
                }
                else if (c == 'q') goto delete_buffer;
                else if (c == 'd') goto delete_buffer;
                else if (c == '?') {
                    Matrix usage_matrix = init_matrix(g_usage, g_iu_fp);
                    buffers.matrices[buffers.len] = usage_matrix;
                    buffers.last_viewed_lines[buffers.len++] = 0;
                    b_idx = buffers.len-1;
                    paths.actual[paths.len++] = g_iu_fp;
                    goto switch_buffer;
                }
                else if (c == 'Q' || c == 'D') {
                    reset_scrn();
                    free(matrix->data);
                    goto end;
                }
                else if ((c == 'l' || c == 'K') && b_idx < buffers.len-1) {
                    buffers.last_viewed_lines[b_idx++] = line;
                    goto switch_buffer;
                }
                else if ((c == 'h' || c == 'J') && b_idx > 0) {
                    buffers.last_viewed_lines[b_idx--] = line;
                    goto switch_buffer;
                }
                else redraw_matrix(matrix, line);
            } break;
            case USER_INPUT_TYPE_UNKNOWN: {} break;
            default: {} break;
            }

            display_tabs(matrix, paths.actual, paths.len, buffers.last_viewed_lines, line);
        }

    delete_buffer:
        if (strcmp(matrix->filepath, g_ob_fp) != 0
            && strcmp(matrix->filepath, g_iu_fp) != 0)
            free(matrix->data);

        for (size_t j = b_idx; j < buffers.len - 1; ++j) {
            buffers.matrices[j] = buffers.matrices[j + 1];
            buffers.last_viewed_lines[j] = buffers.last_viewed_lines[j + 1];
            paths.actual[j] = paths.actual[j+1];
        }

        --paths.len;
        --buffers.len;

        if (b_idx >= buffers.len && buffers.len > 0)
            --b_idx;

        if (buffers.len == 0) {
            reset_scrn();
            goto end;
        }

        goto switch_buffer;

    switch_buffer:
        (void)0x0;
    }

 end:
    return 0;
}
