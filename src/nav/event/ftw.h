#ifndef FN_EVENT_FTW_H
#define FN_EVENT_FTW_H

#include "nav/event/file.h"

void ftw_init();
void ftw_cleanup();
void ftw_add(char *src, char *dest, Buffer *, int);
void ftw_cancel();

#endif
