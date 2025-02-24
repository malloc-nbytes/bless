#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "matrix.h"
#include "io.h"
#include "control.h"
#include "bless-config.h"
#include "utils.h"
#include "flags.h"

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

void dump_matrix(const Matrix *const matrix, size_t start_row, size_t end_row) {
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

void redraw_matrix(Matrix *matrix, size_t line) {
    reset_scrn();
    dump_matrix(matrix, line, g_win_height);
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

        if (strcmp(g_editor, "vim") == 0 || strcmp(g_editor, "nvim") == 0) {
            snprintf(line_arg, sizeof(line_arg), "+%zu", line + 1);
            execlp(g_editor, g_editor, line_arg, matrix->filepath, NULL);
        } else if (strcmp(g_editor, "nano") == 0) {
            snprintf(line_arg, sizeof(line_arg), "+%zu", line + 1);
            execlp("nano", "nano", line_arg, matrix->filepath, NULL);
        } else if (strcmp(g_editor, "vscode") == 0) {
            snprintf(line_arg, sizeof(line_arg), "--goto");
            execlp("code", "code", line_arg, matrix->filepath, NULL);
        } else if (strcmp(g_editor, "emacs") == 0) {
            snprintf(line_arg, sizeof(line_arg), "+%zu", line + 1);
            execlp("emacs", "emacs", line_arg, matrix->filepath, NULL);
        } else {
            fprintf(stderr, "Error: Unsupported editor `%s`\n", g_editor);
            _exit(1);
        }

        perror("execlp failed"); // If execlp fails
        _exit(1);
    } else if (pid > 0) { // Parent process
        wait(NULL); // Wait for the editor to exit
    } else {
        perror("fork failed");
    }

    free(matrix->data);
    *matrix = init_matrix(file_to_cstr(matrix->filepath), matrix->filepath);
    reset_scrn();
    dump_matrix(matrix, line, g_win_height);
}
