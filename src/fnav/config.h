#ifndef FN_CONFIG_H
#define FN_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

void config_init();
bool config_load(const char* file);
bool info_load(const char *file);
bool config_read(FILE *file);
bool info_read(FILE *file);
char* strip_whitespace(char *str);

#endif
