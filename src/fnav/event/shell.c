// shell

#include <malloc.h>

#include "fnav/event/shell.h"
#include "fnav/event/wstream.h"
#include "fnav/model.h"
#include "fnav/log.h"
#include "fnav/config.h"

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
  bool eof);
static void shell_loop(Loop *loop, int ms);
static void shell_write_cb(Stream *stream, void *data, int status);
static void shell_write(Shell *sh, String msg);

void shell_fin_cb(Process *proc, void *data)
{
  log_msg("SHELL", "fin");
  Shell *sh = (Shell*)data;
  sh->blocking = false;
  if (sh->again) {
    log_msg("SHELL", "again");
    sh->again = false;
    shell_start(sh);
  }
  if (sh->disposable)
    shell_free(sh);
}

Shell* shell_init(Cntlr *cntlr)
{
  log_msg("SHELL", "init");
  Shell *sh = malloc(sizeof(Shell));
  sh->buf.data = NULL;
  sh->buf.cap = 0;
  sh->buf.len = 0;
  sh->data_cb = out_data_cb;
  sh->caller = cntlr;
  sh->blocking = false;
  sh->again = false;
  sh->msg = NULL;

  loop_add(&sh->loop, shell_loop);
  uv_timer_init(&sh->loop.uv, &sh->loop.delay);

  sh->uvproc = uv_process_init(&sh->loop, sh);
  sh->proc = &sh->uvproc.process;
  sh->proc->events = &sh->loop.events;
  sh->proc->in = &sh->in;
  sh->proc->out = &sh->out;
  sh->proc->fin_cb = shell_fin_cb;
  return sh;
}

void shell_args(Shell *sh, String *args, shell_stdout_cb readout)
{
  sh->proc->argv = args;
  sh->readout = readout;
}

void shell_free(Shell *sh)
{
  if (sh->msg) free(sh->msg);
  process_teardown(&sh->loop);
  loop_remove(&sh->loop);
  free(sh);
}

void shell_start(Shell *sh)
{
  log_msg("SHELL", "start");
  if (sh->blocking) {
    log_msg("SHELL", "|***BLOCKED**|");
    sh->again = true;
    return;
  }
  if (!process_spawn(sh->proc)) {
    log_msg("PROC", "cannot execute");
    return;
  }
  sh->proc->in->events = NULL;
  sh->blocking = true;
  wstream_init(sh->proc->in, 0);
  if (sh->readout) {
    rstream_init(sh->proc->out, &sh->loop.events, 0);
    rstream_start(sh->proc->out, sh->data_cb);
  }
  if (sh->msg)
    shell_write(sh, sh->msg);
}

void shell_stop(Shell *sh)
{
  log_msg("SHELL", "stop");
  process_stop(sh->proc);
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

  if (!wstream_write(&sh->in, input_buffer)) {
    // couldn't write, stop the process and tell the user about it
    log_msg("SHELL", "couldn't write");
    process_stop(sh->proc);
    return;
  }
  // close the input stream after everything is written
  wstream_set_write_cb(&sh->in, shell_write_cb);
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
  if (sh->caller)
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

static void shell_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

static void shell_write_cb(Stream *stream, void *data, int status)
{
  stream_close(stream, NULL);
}

void shell_exec(String line)
{
  log_msg("SHELL", "shell_exec");
  log_msg("SHELL", "%s", line);
  if (strlen(line) < 2) return;
  line = strip_whitespace(line);
  line = &line[1]; // skip '!'

  String cmds = strtok(line, "|");
  String name = strtok(cmds, " ");
  String args = strtok(NULL, " ");

  Shell *sh = shell_init(NULL);
  sh->disposable = true;
  char *rv[] = {name, args, NULL};
  sh->proc->argv = rv;
  shell_start(sh);
}
