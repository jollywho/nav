#ifndef FN_EVENT_FTW_H
#define FN_EVENT_FTW_H

#include "nav/event/file.h"

void ftw_init();
void ftw_cleanup();
void ftw_add(char*, char *, FileGroup*);
void ftw_add_again(char*, char *, FileGroup*);
void ftw_cancel();
void ftw_retry();

#endif
