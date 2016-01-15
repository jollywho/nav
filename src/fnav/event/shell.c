// shell
#include "fnav/event/shell.h"
#include "fnav/event/wstream.h"
#include "fnav/event/event.h"
#include "fnav/model.h"
#include "fnav/log.h"
#include "fnav/option.h"

#define MAX_PROC 10

static Shell shells[MAX_PROC+1];
static int sh_count = 0;

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
  bool eof);
static void shell_write_cb(Stream *stream, void *data, int status);
static void shell_write(Shell *sh, String msg);
static void shell_default_stdout_cb(Cntlr *cntlr, String out);

static void process_exit(Process *proc, int status, void *data)
{
  log_msg("SHELL", "fin");
  Shell *sh = (Shell*)data;
  if (!sh) return;
  sh->blocking = false;
  if (sh->again) {
    log_msg("SHELL", "again");
    sh->again = false;
    shell_start(sh);
  }
  if (sh->reg) {
    if (!sh->data.process.uvproc.process.closed) {
      process_stop(&sh->data.process.uvproc.process);
    }
    sh_count--;
  }
}

Shell* shell_init(Cntlr *cntlr)
{
  log_msg("SHELL", "init");
  Shell *sh = malloc(sizeof(Shell));
  sh->msg = NULL;
  sh->readout = NULL;
  sh->blocking = false;
  sh->again = false;
  sh->reg = false;
  sh->data_cb = out_data_cb;

  sh->caller = cntlr;
  sh->data.process.uvproc = uv_process_init(mainloop(), sh);
  Process *proc = &sh->data.process.uvproc.process;
  proc->events = eventq();

  proc->in = &sh->data.process.in;
  proc->out = &sh->data.process.out;
  proc->err = &sh->data.process.err;
  proc->cb = process_exit;
  return sh;
}

void shell_args(Shell *sh, String *args, shell_stdout_cb readout)
{
  Process *proc = &sh->data.process.uvproc.process;
  proc->argv = args;
  sh->readout = readout;
}

void shell_free(Shell *sh)
{
  log_msg("SHELL", "shell_free");
  if (sh->msg)  free(sh->msg);
  free(sh);
}

void shell_start(Shell *sh)
{
  log_msg("SHELL", "start");
  Process *proc = &sh->data.process.uvproc.process;
  if (sh->blocking) {
    log_msg("SHELL", "|***BLOCKED**|");
    sh->again = true;
    return;
  }
  if (!process_spawn(proc)) {
    log_msg("PROC", "cannot execute");
    if (sh->reg)
      sh_count--;
    return;
  }
  sh->blocking = true;
  wstream_init(proc->in, 0);
  if (sh->readout) {
    rstream_init(proc->out, eventq(), 0);
    rstream_start(proc->out, sh->data_cb);
  }
  if (sh->msg)
    shell_write(sh, sh->msg);
}

void shell_stop(Shell *sh)
{
  log_msg("SHELL", "stop");
  Process *proc = &sh->data.process.uvproc.process;
  process_stop(proc);
}

void shell_set_in_buffer(Shell *sh, String msg)
{
  log_msg("SHELL", "shell_write");
  if (sh->msg) free(sh->msg);
  sh->msg = strdup(msg);
}

static void shell_write(Shell *sh, String msg)
{
  log_msg("SHELL", "write_to_stream");
  WBuffer *input_buffer = wstream_new_buffer(msg, strlen(msg), 1, NULL);
  Process *proc = &sh->data.process.uvproc.process;

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

  if (!cnt) {
    return;
  }

  ptr[count] = '\0';
  Shell *sh = (Shell*)data;
  sh->readout(sh->caller, ptr);

  size_t written = count;
  // No output written, force emptying the Rbuffer if it is full.
  if (!written && rbuffer_size(buf) == rbuffer_capacity(buf)) {
    written = cnt;
  }
  if (written) {
    rbuffer_consumed(buf, count);
  }
}

static void shell_write_cb(Stream *stream, void *data, int status)
{
  stream_close(stream, NULL);
}

static void shell_default_stdout_cb(Cntlr *cntlr, String out)
{
  log_msg("SHELL", "stdout: %s", out);
}

void shell_exec(String line)
{
  log_msg("SHELL", "shell_exec");
  log_msg("SHELL", "%s", line);
  if (strlen(line) < 2) return;

  if (sh_count + 1 > MAX_PROC) {
    log_msg("SHELL", "**** MAX PROC. CANNOT LAUNCH****");
    return;
  }

  Shell *sh = &shells[sh_count];
  sh->data.process.uvproc = uv_process_init(mainloop(), sh);
  sh->msg = NULL;
  sh->readout = NULL;
  sh->blocking = false;
  sh->again = false;
  sh->reg = true;
  sh->data_cb = out_data_cb;

  Process *proc = &sh->data.process.uvproc.process;
  proc->events = eventq();

  proc->in = &sh->data.process.in;
  proc->out = &sh->data.process.out;
  proc->err = &sh->data.process.err;
  proc->cb = process_exit;

  String rv = strdup(&line[1]);

  char* args[4] = {p_sh, "-c", rv, NULL};
  shell_args(sh, args, shell_default_stdout_cb);
  shell_start(sh);
  sh_count++;
  free(rv);
}
