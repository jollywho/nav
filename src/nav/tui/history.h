#ifndef FN_HIST_MENU_H
#define FN_HIST_MENU_H

#include "nav/cmd.h"

typedef struct fn_hist fn_hist;

fn_hist* hist_new();
void hist_delete(fn_hist* hst);

void hist_push(fn_hist *hst, Cmdline *cmd);
void hist_pop(fn_hist *hst);
void hist_save(fn_hist *hst);
void hist_insert(char *);

void role_call(fn_hist *hst);

const char* hist_prev(fn_hist *hst);
const char* hist_next(fn_hist *hst);

#endif