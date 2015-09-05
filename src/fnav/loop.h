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

#include "rpc.h"

void queue_init();
void queue_push(Job job);

#endif
