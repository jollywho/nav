#ifndef FN_CONFIG_H
#define FN_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

void config_init();
void config_load(const char *);
void config_load_defaults();
void config_write_info();
bool config_read(FILE *);
bool info_read(FILE *);
char* strip_whitespace(char *);

#endif
