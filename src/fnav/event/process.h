#ifndef FN_EVENT_PROCESS_H
#define FN_EVENT_PROCESS_H

#include "fnav/event/loop.h"
#include "fnav/event/stream.h"

extern char *p_sh;          /* 'shell' */
typedef enum {
  kProcessTypeUv,
  kProcessTypePty
} ProcessType;

typedef struct process Process;
typedef void (*process_exit_cb)(Process *proc, int status, void *data);
typedef void (*internal_process_cb)(Process *proc);

struct process {
  ProcessType type;
  Loop *loop;
  void *data;
  int pid, status, refcount;
  // set to the hrtime of when process_stop was called for the process.
  uint64_t stopped_time;
  char **argv;
  Stream *in, *out, *err;
  process_exit_cb cb;
  internal_process_cb internal_exit_cb, internal_close_cb;
  bool closed, term_sent;
  Queue *events;
  SLIST_ENTRY(process) ent;
};

static inline Process process_init(Loop *loop, ProcessType type, void *data)
{
  return (Process) {
    .type = type,
    .data = data,
    .loop = loop,
    .pid = 0,
    .status = 0,
    .refcount = 0,
    .stopped_time = 0,
    .argv = NULL,
    .in = NULL,
    .out = NULL,
    .err = NULL,
    .cb = NULL,
    .closed = false,
    .term_sent = false,
    .internal_close_cb = NULL,
    .internal_exit_cb = NULL,
  };
}
bool process_spawn(Process *proc);
void process_teardown(Loop *loop);
void process_close_streams(Process *proc);
void process_close_in(Process *proc);
void process_close_out(Process *proc);
void process_close_err(Process *proc);
void process_stop(Process *proc);

#endif
