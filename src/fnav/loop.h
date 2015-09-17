#ifndef FN_CORE_LOOP_H
#define FN_CORE_LOOP_H
// Spin loop for selective async processing.
//
// Jobs are created by rpc and then pushed to the job queue.
// The job queue spins until no jobs remain. A timeout is
// checked between jobs that runs the event queue so that
// input is not blocked for too long.
//
//                    +------------+         +-----+
//  STDIN------------>| Event Loop |-------->| RPC |
//          ^         +------------+         +-----+
//          |                |                  |
//       timeout          timeout              job
//          |                |                  |
//          |                v                  v
//          |         +------------+        +-------+
//          +-------->| Spin Loop  |<-------| Queue |
//          |         +------------+        +-------+
//          |              |
//          |         ##Run Job##
//          |              |
//          +------<-------+

#include "fnav/table.h"

typedef struct queue Queue;
struct queue {
  Queue *parent;
  QUEUE headtail;
};

#define qhead(q) Queue qhead_##q
qhead(work);
qhead(draw);

void queue_init();
void queue_push(Queue *queue, Job *job, JobArg *arg);

#define QUEUE_PUT(q,j,ja) \
  queue_push(&qhead_##q, j, ja)

#endif
