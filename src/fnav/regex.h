#ifndef FN_REGEX_H
#define FN_REGEX_H

#include "fnav/cmdline.h"

typedef struct LineMatch LineMatch;

LineMatch* regex_new(fn_handle *hndl);
void regex_destroy(fn_handle *hndl);
void regex_build(LineMatch *lm, String line);
void regex_del_matches(LineMatch *lm);

void regex_mk_pivot(LineMatch *lm);
void regex_pivot(LineMatch *lm);

void regex_hover(LineMatch *lm);
void regex_next(LineMatch *lm, int line);
void regex_prev(LineMatch *lm, int line);

#endif
