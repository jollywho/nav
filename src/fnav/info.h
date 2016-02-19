#ifndef FN_INFO_H
#define FN_INFO_H

#include <stdio.h>
#include "fnav/fnav.h"

void info_write_file(FILE *file);
void info_parse(char *);

char* mark_path(const char *);
char* mark_str(int chr);

void mark_label_dir(char *, const char *);
void mark_strchr_str(const char *, const char *);
void mark_chr_str(int chr, const char *);

void mark_list(List *args);
void marklbl_list(List *args);

#endif
