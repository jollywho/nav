#ifndef FN_REGEX_H
#define FN_REGEX_H

#include "fnav/cmdline.h"

typedef struct Regexmatch Regexmatch;

void regex_build(String line);
void regex_destroy(Buffer *buf);

void regex_start_pivot();
void regex_stop_pivot();
void regex_pivot();

void regex_next();
void regex_prev();

#endif
