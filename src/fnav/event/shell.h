#ifndef FN_EVENT_SHELL_H
#define FN_EVENT_SHELL_H

#include "fnav/event/uv_process.h"
#include "fnav/event/rstream.h"
#include "fnav/log.h"

typedef struct {
  char *data;
  size_t cap, len;
} DynamicBuffer;

typedef struct {
  Loop loop;
  Stream in, out, err;
  DynamicBuffer buf;
  stream_read_cb data_cb;
  UvProcess ptyproc;
  Process *proc;
} Shell;

Shell* shell_init(Cntlr *c);
void shell_free(Shell *sh);
void shell_start(Shell *sh, String *args);
void shell_stop(Shell *sh);
void shell_write(Shell *sh, String msg);

#endif
