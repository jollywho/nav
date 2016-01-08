#ifndef FN_HIST_MENU_H
#define FN_HIST_MENU_H

#include "fnav/cmd.h"

typedef struct fn_hist fn_hist;

fn_hist* hist_new();
void hist_delete(fn_hist* hst);

void hist_push(fn_hist *hst, Cmdline *cmd);
void hist_pop(fn_hist *hst);
void hist_save(fn_hist *hst);

void role_call(fn_hist *hst);

String hist_prev(fn_hist *hst);
String hist_next(fn_hist *hst);

#endif
