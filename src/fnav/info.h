#ifndef FN_INFO_H
#define FN_INFO_H

#include <stdio.h>
#include "fnav/fnav.h"

void info_write_file(FILE *file);
void info_parse(String line);

String mark_path(String key);
String mark_str(int chr);

void mark_label_dir(String label, String dir);
void mark_strchr_str(String str, String dir);
void mark_chr_str(int chr, String dir);

void mark_list(List *args);
void marklbl_list(List *args);

#endif
