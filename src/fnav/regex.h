#ifndef FN_REGEX_H
#define FN_REGEX_H

#include "fnav/cmdline.h"

typedef struct Regexmatch Regexmatch;

void regex_build(String line);
void regex_destroy(Buffer *buf);

void regex_mk_pivot();
void regex_swap_pivot();
void regex_pivot();

void regex_hover();
void regex_next(int line);
void regex_prev(int line);

#endif
