#include "nav/event/shell.h"
#include "nav/event/wstream.h"
#include "nav/event/event.h"
#include "nav/event/hook.h"
#include "nav/log.h"
#include "nav/option.h"
#include "nav/plugins/out/out.h"
#include "nav/plugins/op/op.h"

static void out_data_cb(Stream *, RBuffer *, size_t, void *,  bool);
static void shell_write_cb(Stream *, void *, int);

static char *args[4];

static void process_exit(Process *proc, int status, void *data)
{
  log_msg("SHELL", "fin");
  Shell *sh = data;
  int pid = sh->uvproc.process.pid;
  if (!sh)
    return;

  op_set_exit_status(status);

  sh->blocking = false;
  if (sh->again) {
    log_msg("SHELL", "again");
    sh->again = false;
    shell_start(sh);
  }
  if (sh->reg)
    free(sh);
  send_hook_msg("execclose", NULL, NULL, &(HookArg){NULL,&pid});
}

Shell* shell_new(Plugin *plugin)
{
  log_msg("SHELL", "init");
  Shell *sh = malloc(sizeof(Shell));
  sh->msg = NULL;
  sh->readout = NULL;
  sh->blocking = false;
  sh->again = false;
  sh->reg = false;
  sh->data_cb = out_data_cb;

  sh->caller = plugin;
  sh->uvproc = uv_process_init(mainloop(), sh);
  Process *proc = &sh->uvproc.process;
  proc->events = eventq();

  proc->in = &sh->in;
  proc->out = &sh->out;
  proc->err = &sh->err;
  proc->cb = process_exit;
  return sh;
}

void shell_args(Shell *sh, char **args, shell_stdout_cb readout)
{
  Process *proc = &sh->uvproc.process;
  proc->argv = args;
  sh->readout = readout;
}

void shell_delete(Shell *sh)
{
  log_msg("SHELL", "shell_free");
  if (sh->msg)
    free(sh->msg);
  free(sh);
}

void shell_start(Shell *sh)
{
  log_msg("SHELL", "start");
  Process *proc = &sh->uvproc.process;
  if (sh->blocking) {
    log_msg("SHELL", "|***BLOCKED**|");
    sh->again = true;
    return;
  }
  int succ = process_spawn(proc);
  if (!succ) {
    log_msg("PROC", "cannot execute %d", succ);
    if (sh->reg)
      process_exit(&sh->uvproc.process, 0, 0);
    return;
  }

  op_set_exec_line(proc->argv[2], NULL);
  sh->blocking = true;
  wstream_init(proc->in, 0);
  rstream_init(proc->out, 0);
  rstream_start(proc->out, sh->data_cb);
  rstream_init(proc->err, 0);
  rstream_start(proc->err, sh->data_cb);
  if (sh->msg)
    shell_write(sh, sh->msg);
}

void shell_stop(Shell *sh)
{
  log_msg("SHELL", "stop");
  Process *proc = &sh->uvproc.process;
  process_stop(proc);
}

void shell_set_in_buffer(Shell *sh, char *msg)
{
  log_msg("SHELL", "shell_write");
  if (sh->msg)
    free(sh->msg);
  sh->msg = strdup(msg);
}

void shell_write(Shell *sh, char *msg)
{
  log_msg("SHELL", "write_to_stream");
  WBuffer *input_buffer = wstream_new_buffer(msg, strlen(msg), 1, NULL);
  Process *proc = &sh->uvproc.process;

  if (!wstream_write(proc->in, input_buffer)) {
    // couldn't write, stop the process and tell the user about it
    log_msg("SHELL", "couldn't write");
    process_stop(proc);
    return;
  }
  // close the input stream after everything is written
  wstream_set_write_cb(proc->in, shell_write_cb);
}

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
  bool eof)
{
  log_msg("SHELL", "out_data_cb");
  size_t cnt;
  char *ptr = rbuffer_read_ptr(buf, &cnt);

  if (!cnt)
    return;

  ptr[count] = '\0';
  Shell *sh = (Shell*)data;
  int pid = sh->uvproc.process.pid;
  int fd = &sh->out == stream ? 1 : 2;

  if (sh->readout)
    sh->readout(sh->caller, ptr);
  else
    out_recv(pid, fd, count, ptr);

  size_t written = count;
  // No output written, force emptying the Rbuffer if it is full.
  if (!written && rbuffer_size(buf) == rbuffer_capacity(buf))
    written = cnt;
  if (written)
    rbuffer_consumed(buf, count);
}

static void shell_write_cb(Stream *stream, void *data, int status)
{
  stream_close(stream, NULL);
}

int shell_exec(char *line, char *cwd)
{
  log_msg("SHELL", "shell_exec");
  log_msg("SHELL", "%s", line);

  Shell *sh = malloc(sizeof(Shell));
  sh->msg = NULL;
  sh->readout = NULL;
  sh->blocking = false;
  sh->again = false;
  sh->reg = true;
  sh->data_cb = out_data_cb;
  sh->caller = NULL;

  sh->uvproc = uv_process_init(mainloop(), sh);
  Process *proc = &sh->uvproc.process;
  proc->events = eventq();

  proc->in = &sh->in;
  proc->out = &sh->out;
  proc->err = &sh->err;
  proc->cb = process_exit;
  proc->cwd = cwd;

  char *rv = strdup(line);
  args[0] = get_opt_str("shell");
  args[1] = "-c";
  args[2] = rv;
  args[3] = NULL;
  shell_args(sh, args, NULL);
  shell_start(sh);
  free(rv);

  int pid = sh->uvproc.process.pid;
  send_hook_msg("execopen", NULL, NULL, &(HookArg){NULL,&pid});
  return pid;
}
