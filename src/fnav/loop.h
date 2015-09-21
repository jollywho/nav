#ifndef FN_CORE_LOOP_H
#define FN_CORE_LOOP_H

#include "fnav/table.h"

typedef struct queue Queue;
struct queue {
  Queue *parent;
  QUEUE headtail;
};

#define qhead(q) Queue qhead_##q
qhead(main);
qhead(work);
qhead(draw);

void queue_init();
void queue_push(Queue *queue, Job *job, JobArg *arg);

#define QUEUE_PUT(q,j,ja) \
  queue_push(&qhead_##q, j, ja)

#endif
