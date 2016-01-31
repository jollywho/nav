#ifndef FN_EVENT_SHELL_H
#define FN_EVENT_SHELL_H

#include "fnav/event/uv_process.h"
#include "fnav/event/rstream.h"
#include "fnav/plugins/plugin.h"

typedef void (*shell_stdout_cb)(Plugin *c, String out);

typedef struct Shell Shell;
struct Shell {
  stream_read_cb data_cb;
  UvProcess uvproc;
  Stream in;
  Stream out;
  Stream err;

  shell_stdout_cb readout;
  Plugin *caller;
  bool blocking;
  bool again;
  bool reg;
  int tbl_idx;
  String msg;
};

void shell_init();
void shell_cleanup();
Shell* shell_new(Plugin *plugin);
void shell_delete(Shell *sh);
void shell_args(Shell *sh, String *args, shell_stdout_cb readout);
void shell_start(Shell *sh);
void shell_stop(Shell *sh);
void shell_set_in_buffer(Shell *sh, String msg);
void shell_exec(String line);
void shell_write(Shell *sh, String msg);

#endif
