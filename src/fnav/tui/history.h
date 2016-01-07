#ifndef FN_HIST_MENU_H
#define FN_HIST_MENU_H

#include "fnav/fnav.h"

typedef struct fn_hist fn_hist;

fn_hist* hist_new();

void hist_push(fn_hist *hst, String line);
void hist_pop(fn_hist *hst);
void hist_save(fn_hist *hst, String line);

void role_call(fn_hist *hst);

String hist_prev(fn_hist *hst);
String hist_next(fn_hist *hst);

#endif
