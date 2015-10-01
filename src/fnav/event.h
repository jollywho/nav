#ifndef FN_CORE_EVENT_H
#define FN_CORE_EVENT_H

#include "fnav/loop.h"

int event_init(void);
void start_event_loop(void);
void cycle_events(int ms);
void stop_cycle(void);
Loop *eventloop(void);

#endif
