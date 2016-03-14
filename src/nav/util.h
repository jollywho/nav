#ifndef FN_UTIL_H
#define FN_UTIL_H

#include <ncurses.h>

void trans_str_wide(char *src, cchar_t dst[], int max);
void readable_fs(double size/*in bytes*/, char buf[]);
void expand_escapes(char *dest, const char *src);
void del_param_list(char **params, int argc);
char* do_expansion(char *line, char* (*expfn)(char*));

#endif
