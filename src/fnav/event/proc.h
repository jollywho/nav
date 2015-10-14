#ifndef FN_CORE_PROC_H
#define FN_CORE_PROC_H

#include "fnav/event/loop.h"
#include "fnav/event/stream.h"

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
};

typedef struct uv_process {
  Process process;
  uv_process_t uv;
  uv_process_options_t uvopts;
  uv_stdio_container_t uvstdio[3];
} UvProcess;

#endif
