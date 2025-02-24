#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"

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

const char *get_line_from_file_cstr(const char *fp, size_t lineno) {
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
