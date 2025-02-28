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

#include "control.h"
#include "flags.h"
#include "io.h"
#include "matrix.h"
#include "utils.h"
#include "bless-config.h"

#define BUFFERS_LIM 256

#define DEF_WIN_WIDTH 80
#define DEF_WIN_HEIGHT 24

char *g_ob_fp = "bless-open-buffer";
char *g_iu_fp = "bless-usage";
char *g_usage = "Bless internal usage buffer:\n\n"
"__________.__                        \n"
"\\______   \\  |   ____   ______ ______\n"
" |    |  _/  | _/ __ \\ /  ___//  ___/\n"
" |    |   \\  |_\\  ___/ \\___ \\ \\___ \\ \n"
" |______  /____/\\___  >____  >____  >\n"
"        \\/          \\/     \\/     \\/ \n\n"
    "Quick navigation:\n"
    "C-n or j or [DOWN] for scroll down\n"
    "C-p or k or [UP] for scroll up\n"
    "C-f or l or [RIGHT] for scroll right\n"
    "C-b or h or [LEFT] for scroll left\n\n"

    "Command Sequences\n"
    "    :               Special input mode\n"
    "    :search         Enable search mode\n"
    "    :q              Quit buffer\n"
    "    :w              Save buffer\n"
    "    :<number>       Jump to line number\n\n"

    "Buffer Navigation\n"
    "    j               Scroll Down\n"
    "    C-n             Scroll Down\n"
    "    [DOWN]          Scroll down\n\n"

    "    k               Scroll Up\n"
    "    C-p             Scroll Up\n"
    "    [UP]            Scroll up\n\n"

    "    h               Scroll left\n"
    "    C-b             Scroll left\n"
    "    [LEFT]          Scroll left\n\n"

    "    l               Scroll right\n"
    "    C-f             Scroll right\n"
    "    [RIGHT]         Scroll right\n\n"

    "    z               Put the top line in the center of the screen\n"
    "    C-l             Put the top line in the center of the screen\n\n"

    "    C-d             Page down\n"
    "    C-v             Page down\n\n"

    "    C-u             Page up\n"
    "    M-v             Page up\n\n"

    "    g               Jump to top of page\n"
    "    G               Jump to bottom of page\n\n"

    "    $               Jump to end of line\n"
    "    C-e             Jump to end of line\n\n"

    "    0               Jump to beginning of line\n"
    "    C-a             Jump to beginning of line\n\n"

    "    /               Enable search\n"
    "    C-s             Enable search\n\n"

    "Search Mode Commands\n"
    "    n               Next match\n\n"

    "    N               Previous match\n"
    "    p               Previous match\n\n"

    "Buffer Controls\n"
    "    q               Quit buffer\n"
    "    d               Quit buffer\n\n"

    "    Q               Quit all buffers\n"
    "    D               Quit all buffers\n\n"

    "    C-w             Save buffer\n"
    "    C-o             Open a saved buffer\n"
    "    I               Open the current line in Vim\n"
    "    L               Redraw buffer\n"
    "    O               Open file in place\n\n"

    "Tab Controls\n"
    "    J               Left buffer\n"
    "    [SHIFT][LEFT]   Left buffer\n\n"

    "    K               Right buffer\n"
    "    [SHIFT][RIGHT]  Right buffer\n\n"

    "Misc\n"
    "    ?               Open this usage buffer\n"
    "    C-g             Cancel\n"

    "";
int            g_win_width      = DEF_WIN_WIDTH;
int            g_win_height     = DEF_WIN_HEIGHT;
char          *g_last_search    = NULL;
uint32_t       g_flags          = 0x0;
char          *g_filter_pattern = NULL;
char          *g_editor         = "vim";
struct termios g_old_termios;
char *g_supported_editors[] = {
    "vim",
    "nvim",
    "nano",
    "emacs",
    "vscode",
};
size_t g_supported_editors_len =
    sizeof(g_supported_editors)/sizeof(*g_supported_editors);

static struct {
    char **paths;
    size_t len, cap;
    size_t *last_saved_lines;
    size_t lsl_len, lsl_cap;
} g_saved_buffers = {0};

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
    raw.c_iflag &= ~IXON;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
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

void save_buffer(Matrix *matrix, size_t line, size_t column) {
    if (!strcmp(matrix->filepath, g_ob_fp) || !strcmp(matrix->filepath, g_iu_fp)) {
        err_msg_wmatrix_wargs(matrix, line, column, "Canot save buffer `%s` as it is internal", matrix->filepath);
        return;
    }
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

char *saved_buffer_contents_create() {
    char *output = calloc(1, 1);
    size_t output_size = 0;

    append_str(&output, &output_size, "=== Saved Buffers ===\n");
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

        append_str(&output, &output_size, "%-4zu [%-*s] %-*s %-*zu\n", i, adjusted_name_len, name, max_path_len, path, 10, line);

        const char *preview = get_line_from_file_cstr(path, line);
        if (preview) {
            while (*preview && *preview == ' ') ++preview;
            //append_str(&output, &output_size, "└────── %s\n", preview);
            append_str(&output, &output_size, "        %s\n", preview);
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

    if (BIT_SET(g_flags, FLAG_TYPE_EDITOR)) {
        int ok = 0;
        for (size_t i = 0; g_supported_editors_len; ++i) {
            if (!strcmp(g_editor, g_supported_editors[i])) {
                ok = 1;
                break;
            }
        }
        if (!ok)
            err_wargs("invalid editor: %s", g_editor);
    }

    struct {
        Matrix matrices[BUFFERS_LIM];
        size_t len;
        size_t last_viewed_lines[BUFFERS_LIM];
    } buffers = {0}; { buffers.len = 0; }

    if (paths.len >= BUFFERS_LIM)
        err_wargs("Only %d files are supported", BUFFERS_LIM);

    for (size_t i = 0; i < paths.len; ++i) {
        if (path_is_dir(paths.actual[i])) {
            size_t files_len = 0;
            char **files_in_dir = walkdir(paths.actual[i], &files_len);

            for (size_t j = 0; j < files_len; ++j)
                da_append(paths.actual, paths.len, paths.cap, char **, files_in_dir[j]);

            // Remove directory listing.
            da_remove(paths.actual, paths.len, i);
        }
    }

    {
        size_t i = 0;
        while (i < paths.len) {
            if (path_is_dir(paths.actual[i])) {
                size_t files_len = 0;
                char **files_in_dir = walkdir(paths.actual[i], &files_len);

                for (size_t j = 0; j < files_len; ++j)
                    da_append(paths.actual, paths.len, paths.cap, char **, files_in_dir[j]);

                // Remove directory listing.
                da_remove(paths.actual, paths.len, i);
            } else {
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
            ++i;
        }
    }

    if (buffers.len == 0) {
        Matrix usage_matrix = init_matrix(g_usage, g_iu_fp);
        buffers.matrices[buffers.len] = usage_matrix;
        buffers.last_viewed_lines[buffers.len++] = 0;
        paths.actual[paths.len++] = g_iu_fp;
    }

    int b_idx = 0, tab = 0;
    while (1) {
        Matrix *matrix = &buffers.matrices[b_idx];

        if (BIT_SET(g_flags, FLAG_TYPE_ONCE)) {
            if (b_idx >= buffers.len)
                break;
            dump_matrix(matrix, 0, matrix->rows, 0, matrix->cols);
            ++b_idx;
            continue;
        }

        size_t line = buffers.last_viewed_lines[b_idx];
        size_t column = 0;
        reset_scrn();
                //                 -1 for tabs
        dump_matrix(matrix, line, g_win_height, column, g_win_width);
        display_tabs(matrix, paths.actual, paths.len, buffers.last_viewed_lines, line, &tab);

        while (1) {
            char c;
            User_Input_Type ty = get_user_input(&c);

            clear_msg();

            int status = 0;

            switch (ty) {
            case USER_INPUT_TYPE_CTRL: {
                if (c == CTRL_N)      handle_scroll_down(matrix, &line, column);
                else if (c == CTRL_P) handle_scroll_up(matrix, &line, column);
                else if (c == CTRL_D) handle_page_down(matrix, &line, column);
                else if (c == CTRL_V) handle_page_down(matrix, &line, column);
                else if (c == CTRL_U) handle_page_up(matrix, &line, column);
                else if (c == CTRL_W) save_buffer(matrix, line, column);
                else if (c == CTRL_L) handle_page_up(matrix, &line, column);
                else if (c == CTRL_F) handle_scroll_right(matrix, line, &column);
                else if (c == CTRL_B) handle_scroll_left(matrix, line, &column);
                else if (c == CTRL_A) handle_jump_to_beginning_of_line(matrix, line, &column);
                else if (c == CTRL_E) handle_jump_to_end_of_line(matrix, line, &column);
                else if (c == CTRL_S) status = handle_search(matrix, &line, line, &column, NULL, 0);
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
                if (c == 'v') handle_page_up(matrix, &line, column);
            } break;
            case USER_INPUT_TYPE_SHIFT_ARROW: {
                if (c == RIGHT_ARROW && b_idx < buffers.len-1) {
                    buffers.last_viewed_lines[b_idx++] = line;
                    tab++;
                    //tab = b_idx; // Move to the next page
                    goto switch_buffer;
                }
                else if (c == LEFT_ARROW && b_idx > 0) {
                    buffers.last_viewed_lines[b_idx--] = line;
                    tab--;
                    goto switch_buffer;
                }
            } break;
            case USER_INPUT_TYPE_ARROW: {
                if (c == UP_ARROW)         handle_scroll_up(matrix, &line, column);
                else if (c == DOWN_ARROW)  handle_scroll_down(matrix, &line, column);
                else if (c == RIGHT_ARROW) handle_scroll_right(matrix, line, &column);
                else if (c == LEFT_ARROW)  handle_scroll_left(matrix, line, &column);
            } break;
            case USER_INPUT_TYPE_NORMAL: {
                if (c == 'k')      handle_scroll_up(matrix, &line, column);
                else if (c == 'j') handle_scroll_down(matrix, &line, column);
                else if (c == 'g') handle_jump_to_top(matrix, &line, column);
                else if (c == 'G') handle_jump_to_bottom(matrix, &line, column);
                else if (c == '/') status = handle_search(matrix, &line, line, &column, NULL, 0);
                else if (c == '0') handle_jump_to_beginning_of_line(matrix, line, &column);
                else if (c == '$') handle_jump_to_end_of_line(matrix, line, &column);
                else if (c == ':') {
                    char *inp = get_user_input_in_mini_buffer(": ", NULL);
                    int is_open_buffer = !strcmp(matrix->filepath, g_ob_fp);
                    if (!inp)
                        break;
                    else if (inp && inp[0] == 'q' && !inp[1])
                        goto delete_buffer;
                    else if (is_open_buffer && isdigit(inp[0])) {
                        int idx = atoi(inp);
                        Matrix selected_matrix = init_matrix(file_to_cstr(g_saved_buffers.paths[idx]), g_saved_buffers.paths[idx]);
                        buffers.matrices[buffers.len] = selected_matrix;
                        buffers.last_viewed_lines[buffers.len++] = g_saved_buffers.last_saved_lines[idx];
                        paths.actual[paths.len++] = selected_matrix.filepath;
                        goto delete_buffer;
                    } else if (isdigit(inp[0])) {
                        handle_jump_to_line_num(matrix, &line, column, atoi(inp));
                    } else if (is_open_buffer && inp && inp[0] == 'r') {
                        int idx = atoi(inp+1);
                        remove_entry_from_config_file(idx);
                        free(matrix->data);
                        char *saved_buffer_contents = saved_buffer_contents_create();
                        *matrix = init_matrix(saved_buffer_contents, g_ob_fp);
                        goto switch_buffer;
                    }
                    else if (inp[0] == 'w' && !inp[1])
                        save_buffer(matrix, line, column);
                    else if (!strcmp(inp, "search"))
                        status = handle_search(matrix, &line, line, &column, NULL, 0);
                    else if (!strcmp(inp, "searchjmp"))
                        status = jump_to_last_searched_word(matrix, &line, &column, 0);
                    else {
                        status = MATRIX_ACTION_NOT_A_VALID_CMD_SEQ;
                    }
                }
                else if (c == 'n') status = jump_to_last_searched_word(matrix, &line, &column, 0);
                else if (c == 'N' || c == 'p') status = jump_to_last_searched_word(matrix, &line, &column, 1);
                else if (c == 'I') launch_editor(matrix, line, column);
                else if (c == 'L') redraw_matrix(matrix, line, column);
                else if (c == 'z') handle_page_up(matrix, &line, column);
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
                else if (c == 'l') handle_scroll_right(matrix, line, &column);
                else if (c == 'h') handle_scroll_left(matrix, line, &column);
                else if (c == 'K' && b_idx < buffers.len-1) {
                    buffers.last_viewed_lines[b_idx++] = line;
                    ++tab;
                    goto switch_buffer;
                }
                else if (c == 'J' && b_idx > 0) {
                    buffers.last_viewed_lines[b_idx--] = line;
                    --tab;
                    goto switch_buffer;
                }
                else redraw_matrix(matrix, line, column);
            } break;
            case USER_INPUT_TYPE_UNKNOWN: {} break;
            default: {} break;
            }

            if (status == MATRIX_ACTION_SEARCH_FOUND) {
                color(BOLD GREEN);
                printf(":search ([n] next) ([N] previous)");
                color(RESET);
                fflush(stdout);
            } else if (status == MATRIX_ACTION_SEARCH_NOT_FOUND) {
                color(RED BOLD);
                printf(":search [Search not found]");
                fflush(stdout);
                color(RESET);
            } else if (status == MATRIX_ACTION_SEARCH_NO_PREV) {
                color(RED BOLD);
                printf(":searchjmp [No previous search]");
                fflush(stdout);
                color(RESET);
            } else if (status == MATRIX_ACTION_NOT_A_VALID_CMD_SEQ) {
                color(RED BOLD);
                printf("[Not a command sequence]");
                fflush(stdout);
                color(RESET);
            } else {
                reset_scrn();
                //                            -1 for tabs
                dump_matrix(matrix, line, g_win_height, column, g_win_width);
                display_tabs(matrix, paths.actual, paths.len, buffers.last_viewed_lines, line, &tab);
            }
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
        //--tab;

        if (b_idx >= buffers.len && buffers.len > 0)
            --b_idx;

        tab = b_idx;

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
