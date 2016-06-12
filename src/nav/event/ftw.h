#ifndef NV_EVENT_FTW_H
#define NV_EVENT_FTW_H

#include "nav/event/file.h"
#define MAX_WAIT 60

void ftw_init();
void ftw_cleanup();
void ftw_add(char*, char *, FileGroup*);
void ftw_add_again(char*, char *, FileGroup*);
void ftw_cancel();
void ftw_retry();

#endif
