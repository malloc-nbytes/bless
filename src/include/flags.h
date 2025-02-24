#ifndef FLAGS_H
#define FLAGS_H

#define FLAG_1HY_HELP    'h'
#define FLAG_1HY_ONCE    'o'
#define FLAG_1HY_LINES   'l'
#define FLAG_1HY_FILTER  'f'
#define FLAG_1HY_EDITOR  'e'
#define FLAG_1HY_VERSION 'v'

#define FLAG_2HY_HELP    "--help"
#define FLAG_2HY_ONCE    "--once"
#define FLAG_2HY_LINES   "--lines"
#define FLAG_2HY_FILTER  "--filter"
#define FLAG_2HY_EDITOR  "--editor"
#define FLAG_2HY_VERSION "--version"

typedef enum {
    FLAG_TYPE_HELP    = 1 << 0,
    FLAG_TYPE_ONCE    = 1 << 1,
    FLAG_TYPE_LINES   = 1 << 2,
    FLAG_TYPE_FILTER  = 1 << 3,
    FLAG_TYPE_EDITOR  = 1 << 4,
    FLAG_TYPE_VERSION = 1 << 5,
} Flag_Type;

void handle_1hy_flag(const char *arg, int *argc, char ***argv);
void handle_2hy_flag(const char *arg, int *argc, char ***argv);
char *eat(int *argc, char ***argv);
void help(void);

#endif // FLAGS_H
