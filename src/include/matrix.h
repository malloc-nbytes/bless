#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

#include "color.h"

typedef struct {
    char *data;
    size_t rows, cols;
    char *filepath;
} Matrix;

#define MAT_AT(m, c, i, j) \
    ((m)[(i) * (c) + (j)])

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

Matrix init_matrix(const char *src, char *filepath);
char *get_user_input_in_mini_buffer(char *prompt, char *last_input);
void dump_matrix(const Matrix *const matrix, size_t start_row, size_t end_row);
void handle_scroll_down(const Matrix *const matrix, size_t *const line);
void handle_scroll_up(const Matrix *const matrix, size_t *const line);
void handle_jump_to_top(const Matrix *const matrix, size_t *const line);
void handle_jump_to_bottom(const Matrix *const matrix, size_t *const line);
int find_word_in_matrix(Matrix *matrix, size_t start_row, char *word, size_t word_len, int reverse);
void handle_search(Matrix *matrix, size_t *line, size_t start_row, char *jump_to_next/*optional*/, int reverse);
void jump_to_last_searched_word(Matrix *matrix, size_t *line, int reverse);
void handle_page_up(Matrix *matrix, size_t *line);
void handle_page_down(Matrix *matrix, size_t *line);
void redraw_matrix(Matrix *matrix, size_t line);
void display_tabs(const Matrix *const matrix,
                  char **paths,
                  size_t paths_len,
                  size_t *last_viewed_lines,
                  size_t line);
void launch_editor(Matrix *matrix, size_t line);

#endif // MATRIX_H
