#ifndef FN_INFO_H
#define FN_INFO_H

#include <stdio.h>
#include "fnav/fnav.h"

void info_write_file(FILE *file);
void info_parse(String line);

void info_set_mark();

String mark_path(String key);
void mark_label_dir(String label, String dir);
void mark_list(List *args);
void marklbl_list(List *args);

#endif
