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

#include "cntlr.h"

struct uv_handle_t;

typedef struct {
  Cntlr *caller;
  void(*cntlr_read_cb)();
  void(*cntlr_after_cb)();
  char **args;
  void(*fn)();
} Job;

typedef struct {
  int num;
  Job *data;
  Job *next;
} QueueItem;

typedef struct {
  QueueItem *head;
  QueueItem *tail;
} Queue;

typedef struct {
  struct uv_handle_t* handle;
  Job *job;
  void(*close_cb)();
} Channel;

void queue_push(Queue *queue, Job job);
void queue_remove(Queue *queue);
