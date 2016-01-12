#ifndef FN_EVENT_SHELL_H
#define FN_EVENT_SHELL_H

#include "fnav/event/uv_process.h"
#include "fnav/event/rstream.h"

typedef struct {
  char *data;
  size_t cap, len;
} DynamicBuffer;

typedef void (*shell_stdout_cb)(Cntlr *c, String out);

typedef struct Shell Shell;
struct Shell {
  Loop *loop;
  Stream in, out, err;
  DynamicBuffer buf;
  stream_read_cb data_cb;
  UvProcess uvproc;
  Process *proc;
  shell_stdout_cb readout;
  Cntlr *caller;
  bool blocking;
  bool again;
  bool disposable;
  String msg;
  Shell *self;
};

Shell* shell_init(Cntlr *cntlr);
void shell_args(Shell *sh, String *args, shell_stdout_cb readout);
void shell_free(Shell *sh);
void shell_start(Shell *sh);
void shell_stop(Shell *sh);
void shell_set_in_buffer(Shell *sh, String msg);
void shell_exec(String line);

#endif
