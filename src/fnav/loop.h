// Spin loop for selective async processing.
//
// Jobs are created by rpc and then pushed to the job queue.
// The job queue spins until no jobs remain. A timeout is
// checked between jobs that runs the event queue so that
// input is not blocked for too long.
//
//                   +------------+        +-----+
//  STDIN----------->| Event Loop |------->| RPC |
//          ^        +------------+        +-----+
//          |              |                  |
//       timeout        timeout               |
//          |              |                  v
//          |              v                  |
//          |        +------------+           |
//          +------->| Spin Loop  |<--------(Job#)
//          |        +------------+
//          |              |
//          |         ##Run Job##
//          |              |
//          +------<-------+

#include "cntlr.h"

typedef struct job_s {
  int active;
  cntlr_t *caller;
  cmd_t *cmd;
} job_t;
