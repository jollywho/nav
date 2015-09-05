#ifndef FN_CORE_EVENT_H
#define FN_CORE_EVENT_H

#include "fnav/loop.h"

int event_init(void);
void start_event_loop(void);
void event_push(Channel channel);
void onetime_event_loop(void);
void stop_event_loop(void);

#endif
