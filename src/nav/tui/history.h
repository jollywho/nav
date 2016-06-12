#ifndef NV_HIST_MENU_H
#define NV_HIST_MENU_H

#include "nav/cmd.h"

typedef struct nv_hist nv_hist;

void hist_init();
void hist_cleanup();
void hist_set_state(int);

void hist_push(int, Cmdline *);
void hist_pop();
void hist_save();
void hist_insert(int, char *);

const char* hist_first();
const char* hist_prev();
const char* hist_next();

#endif
