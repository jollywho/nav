#ifndef FN_REGEX_H
#define FN_REGEX_H

#include "fnav/cmdline.h"

typedef struct Regexmatch Regexmatch;

void regex_build(String line);
void regex_req_enter();
void regex_focus();

void regex_next();
void regex_prev();

#endif
