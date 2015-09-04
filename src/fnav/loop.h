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

#include "lib/queue.h"
#include "cntlr.h"

typedef void(*cntlr_cb)();

typedef struct {
  Cntlr *caller;
  cntlr_cb read_cb;
  cntlr_cb after_cb;
  char **args;
  void(*fn)();
} Job;

typedef struct {
  QUEUE node;
  Job *data;
} QueueItem;

typedef void(*close_cb)();

typedef struct {
  struct uv_handle_t* handle;
  Job *job;
  close_cb cb;
} Channel;

void queue_init();
void queue_push(Job job);

#endif