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
#define CTRL_W 23 // Save buffer
#define CTRL_O 15 // Open buffer

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

static char *g_usage = "Bless internal usage buffer:\n\n"
"__________.__                        \n"
"\\______   \\  |   ____   ______ ______\n"
" |    |  _/  | _/ __ \\ /  ___//  ___/\n"
" |    |   \\  |_\\  ___/ \\___ \\ \\___ \\ \n"
" |______  /____/\\___  >____  >____  >\n"
"        \\/          \\/     \\/     \\/ \n"


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
    "    C-d         Page down\n"
    "    C-u         Page up\n"
    "Emacs Keybindings\n"
    "    C-n         Scroll down\n"
    "    C-p         Scroll up\n"
    "    C-l         Refresh\n"
    "    C-v         Page down\n"
    "    M-v         Page up\n"
    "Agnostic Keybindings\n"
    "    ?           Open this usage buffer\n"
    "    q           Quit buffer\n"
    "    d           Quit buffer\n"
    "    Q           Quit all buffers\n"
    "    D           Quit all buffers\n"
    "    C-w         Save a buffer\n"
    "    C-o         Open a saved buffer\n"
    "    [UP]        Scroll up\n"
    "    [DOWN]      Scroll down\n"
    "    [LEFT]      Left buffer\n"
    "    [RIGHT]     Right buffer\n"
    "";

static int g_win_width  = DEF_WIN_WIDTH;
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
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';  // Remove newline
        }

        char *path = NULL;
        char *line_str = NULL;
        char *name = NULL;

        path = strtok(line, ":");  // First part: file path
        line_str = strtok(NULL, ":");  // Second part: line number
        name = strtok(NULL, ":");  // Third part: name

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

    size_t backspace = prompt_len;

    char *input = (char *)s_malloc(input_lim);
    (void)memset(input, '\0', input_lim);
    size_t input_len = 0;

    if (last_input) {
        out(last_input, 0);
        for (size_t i = 0; last_input[i]; ++i) {
            input[input_len++] = last_input[i];
            ++backspace;
        }
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
                if (backspace > prompt_len) {
                    out("\b \b", 0);
                    input[input_len--] = '\0';
                    --backspace;
                }
            }
            else {
                if (input_len >= input_lim)
                    err_wargs("input length must be < %zu", input_lim);
                input[input_len++] = c;
                ++backspace;
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
    printf("  %s,  -%c    Show line numbers\n", FLAG_2HY_LINES, FLAG_1HY_LINES);
    printf("  %s, -%c    Filter using regex\n", FLAG_2HY_FILTER, FLAG_1HY_FILTER);
    exit(EXIT_FAILURE);
}

void dump_matrix(const Matrix *const matrix, size_t start_row, size_t end_row) {
    if (end_row > matrix->rows)
        end_row = matrix->rows;

    for (size_t i = start_row; i < end_row + start_row; ++i) {
        if (BIT_SET(g_flags, FLAG_TYPE_LINES))
            printf("%zu: ", i+1);
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
        reset_scrn();
        dump_matrix(matrix, *line, g_win_height-1);
        color(RED BOLD UNDERLINE);
        printf("[SEARCH NOT FOUND: %s]\n", actual);
        fflush(stdout);
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
        out("[NO PREVIOUS SEARCH]", 1);
        color(RESET);
        return;
    }
    if (!reverse)
        handle_search(matrix, line, *line+1, g_last_search, 0);
    else
        handle_search(matrix, line, *line-1, g_last_search, 1);
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
            color(GREEN);
            printf(" %s:%zu ", paths[i], line);
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

int open_saved_buffer(Matrix *matrix, size_t *last_off_line) {
    reset_scrn();

    color(BOLD UNDERLINE);
    printf("=== Saved Buffers ===\n");
    color(RESET BOLD);
    printf("Use C-g to cancel\n\n");
    color(RESET);

    char **info_paths = malloc(sizeof(char *) * 10);
    size_t *info_lines = malloc(sizeof(size_t) * 10);
    char **info_names = malloc(sizeof(char *) * 10);
    size_t info_len = 0;

    parse_config_file(&info_paths, &info_lines, &info_names, &info_len);

    // Find the maximum length of name, path, and line
    size_t max_name_len = 0, max_path_len = 0;
    for (size_t i = 0; i < info_len; ++i) {
        size_t name_len = strlen(info_names[i]);
        size_t path_len = strlen(info_paths[i]);
        if (name_len > max_name_len) max_name_len = name_len;
        if (path_len > max_path_len) max_path_len = path_len;
    }

    // Add padding to align the columns
    size_t adjusted_name_len = max_name_len + 2; // 2 for the brackets

    printf("%-4s %-*s %-*s %-*s\n", "Index", adjusted_name_len, "Name", max_path_len, "Path", 10, "Line");
    printf("----------------------------------------------------------------\n");

    for (size_t i = 0; i < info_len; ++i) {
        const char *path = info_paths[i];
        const size_t line = info_lines[i];
        const char *name = info_names[i];

        // Print the information with padding
        color(BOLD);
        printf("%-4zu [%-*s] %-*s %-*zu\n", i, adjusted_name_len, name, max_path_len, path, 10, line);
        color(RESET);
        const char *preview = get_line_from_cstr(path, line);
        if (preview) {
            while (*preview && *preview == ' ') ++preview;
            printf("└────── %s\n", preview);
        }
        else
            printf("[no preview available]");
    }
    putchar('\n');

    char *input = get_user_input_in_mini_buffer("Enter Index: ", NULL);

    if (!input) return 0;

    int idx = atoi(input);

    *matrix = init_matrix(file_to_cstr(info_paths[idx]), info_paths[idx]);
    *last_off_line = info_lines[idx];

    return 1;
}

int main(int argc, char **argv) {
    init_config_file();

    /* if (argc <= 1) */
    /*     help(); */
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
        char *iu_fp = "internal-usage";
        Matrix usage_matrix = init_matrix(g_usage, iu_fp);
        buffers.matrices[buffers.len] = usage_matrix;
        buffers.last_viewed_lines[buffers.len++] = 0;
        paths.actual[paths.len++] = iu_fp;
    }

    int b_idx = 0;
    while (1) {
        Matrix *matrix = &buffers.matrices[b_idx];

        if (BIT_SET(g_flags, FLAG_TYPE_ONCE)) {
            dump_matrix(matrix, 0, matrix->rows);
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
                else if (c == CTRL_O) {
                    size_t left_off_line = 0;
                    Matrix new_matrix = {0};
                    if (!open_saved_buffer(&new_matrix, &left_off_line))
                        goto switch_buffer;
                    buffers.matrices[buffers.len] = new_matrix;
                    buffers.last_viewed_lines[buffers.len++] = left_off_line;
                    b_idx = buffers.len-1;
                    paths.actual[paths.len++] = new_matrix.filepath;
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
                else if (c == 'n') jump_to_last_searched_word(matrix, &line, 0);
                else if (c == 'N') jump_to_last_searched_word(matrix, &line, 1);
                else if (c == 'q') goto delete_buffer;
                else if (c == 'd') goto delete_buffer;
                else if (c == '?') {
                    char *iu_fp = "internal-usage";
                    Matrix usage_matrix = init_matrix(g_usage, iu_fp);
                    buffers.matrices[buffers.len] = usage_matrix;
                    buffers.last_viewed_lines[buffers.len++] = 0;
                    b_idx = buffers.len-1;
                    paths.actual[paths.len++] = iu_fp;
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
