#ifndef FN_EVENT_EVENT_H
#define FN_EVENT_EVENT_H

#include "fnav/event/loop.h"

void event_init(void);
void event_cleanup(void);
void stop_event_loop(void);
void start_event_loop(void);
uv_loop_t *eventloop(void);
uint64_t os_hrtime(void);

#endif
