#ifndef BLESS_CONFIG_H
#define BLESS_CONFIG_H

#include <stdint.h>
#include <stddef.h>

extern char *g_ob_fp;
extern char *g_iu_fp;
extern char *g_usage;
extern char *g_qbuf_fp;

extern int g_win_width;
extern int g_win_height;
extern char *g_last_search;
extern uint32_t g_flags;
extern char *g_filter_pattern;
extern char *g_editor;
extern struct termios g_old_termios;
extern char *g_supported_editors[];
extern size_t g_supported_editors_len;

#endif // BLESS_CONFIG_H
