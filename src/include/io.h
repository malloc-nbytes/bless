#ifndef IO_H
#define IO_H

#include <stddef.h>

char *expand_tilde(const char *path);
const char *file_to_cstr(const char *filename);
const char *get_line_from_file_cstr(const char *fp, size_t lineno);
int path_is_dir(const char *fp);
char **walkdir(const char *dir_path, size_t *len);

#endif // IO_H
