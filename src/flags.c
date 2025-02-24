#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flags.h"
#include "bless-config.h"
#include "utils.h"
#include "config.h"

void help(void) {
    printf("(MIT License) Copyright (c) 2025 malloc-nbytes\n\n");
    printf("Compiler: %s\n", COMPILER_INFO);
    printf("Installed to: %s\n", PREFIX);
    printf("Bless Version: %s\n\n", VERSION);

    printf("Usage: bless [filepath...] [options...]\n");
    printf("Options:\n");
    printf("  %s,   -%c           Print this message\n", FLAG_2HY_HELP, FLAG_1HY_HELP);
    printf("  %s,   -%c           Just print the files (similar to `cat`)\n", FLAG_2HY_ONCE, FLAG_1HY_ONCE);
    printf("  %s,  -%c           Show Bless line numbers (not file line numbers, this is unimplemented)\n", FLAG_2HY_LINES, FLAG_1HY_LINES);
    printf("  %s, -%c <regex>   Filter using regex\n", FLAG_2HY_FILTER, FLAG_1HY_FILTER);
    printf("  %s, -%c <editor>  Change the default editor\n", FLAG_2HY_EDITOR, FLAG_1HY_EDITOR);
    printf("\nValid editors are:\n");
    for (size_t i = 0; i < g_supported_editors_len; ++i)
        printf("    %s\n", g_supported_editors[i]);
    exit(EXIT_SUCCESS);
}

void version(void) {
    printf("Bless version: v" VERSION "\n");
    exit(0);
}

void handle_filter_flag(int *argc, char ***argv) {
    g_filter_pattern = eat(argc, argv);
}

void handle_editor_flag(int *argc, char ***argv) {
    g_editor = eat(argc, argv);
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
        else if (*it == FLAG_1HY_EDITOR) {
            g_flags |= FLAG_TYPE_EDITOR;
            handle_editor_flag(argc, argv);
        }
        else if (*it == FLAG_1HY_VERSION) {
            g_flags |= FLAG_TYPE_VERSION;
            version();
        }
        else
            err_wargs("Unknown option: `%c`", *it);
        ++it;
    }
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
    else if (!strcmp(arg, FLAG_2HY_EDITOR)) {
        g_flags |= FLAG_TYPE_EDITOR;
        handle_editor_flag(argc, argv);
    }
    else if (!strcmp(arg, FLAG_2HY_VERSION)) {
        g_flags |= FLAG_TYPE_VERSION;
        version();
    }
    else
        err_wargs("Unknown option: `%s`", arg);
}

char *eat(int *argc, char ***argv) {
    if (!(*argc))
        return NULL;
    (*argc)--;
    return *(*argv)++;
}
