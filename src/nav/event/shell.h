#ifndef NV_EVENT_SHELL_H
#define NV_EVENT_SHELL_H

#include "nav/event/uv_process.h"
#include "nav/event/rstream.h"
#include "nav/plugins/plugin.h"

typedef void (*shell_stdout_cb)(Plugin *c, char *out);

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
  char *msg;
};

Shell* shell_new(Plugin *plugin);
void shell_delete(Shell *sh);
void shell_args(Shell *sh, char **args, shell_stdout_cb readout);
void shell_start(Shell *sh);
void shell_stop(Shell *sh);
void shell_set_in_buffer(Shell *sh, char *msg);
int shell_exec(char *line, char *cwd);
void shell_write(Shell *sh, char *msg);

#endif
