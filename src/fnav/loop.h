#ifndef FN_CORE_LOOP_H
#define FN_CORE_LOOP_H

#include "fnav/table.h"

void queue_init();
void queue_push(Loop *loop, Job *job, JobArg *arg);

#endif
