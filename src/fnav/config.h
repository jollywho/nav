#ifndef FN_CONFIG_H
#define FN_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

extern char *p_sh;          /* 'shell' */

void config_setup();
bool config_load(const char* file);
bool config_read(FILE *file);
char* strip_whitespace(char *str);

#endif
