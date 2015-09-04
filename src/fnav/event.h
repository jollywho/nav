#ifndef FN_CORE_EVENT_H
#define FN_CORE_EVENT_H

#include "loop.h"

int event_init(void);
void start_event_loop(void);
void event_push(Channel channel);

#endif
