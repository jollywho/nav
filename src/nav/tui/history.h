#ifndef FN_HIST_MENU_H
#define FN_HIST_MENU_H

#include "nav/cmd.h"

typedef struct fn_hist fn_hist;

void hist_init();
void hist_cleanup();
void hist_set_state(int);

void hist_push(int, Cmdline *);
void hist_pop();
void hist_save();
void hist_insert(int, char *);

const char* hist_prev();
const char* hist_next();

#endif
