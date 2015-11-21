// shell

#include <malloc.h>

#include "fnav/event/shell.h"
#include "fnav/event/wstream.h"
#include "fnav/model.h"

char* p_sh = "/bin/sh";

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
  bool eof);
static void shell_loop(Loop *loop, int ms);
static void shell_write_cb(Stream *stream, void *data, int status);

void shell_fin_cb(Process *proc, void *data)
{
  log_msg("SHELL", "fin");
  Shell *sh = (Shell*)data;
  sh->blocking = false;
}

Shell* shell_init(Cntlr *cntlr, shell_stdout_cb readout)
{
  log_msg("SHELL", "init");
  Shell *sh = malloc(sizeof(Shell));
  sh->buf.data = NULL;
  sh->buf.cap = 0;
  sh->buf.len = 0;
  sh->data_cb = out_data_cb;
  sh->caller = cntlr;
  sh->readout = readout;

  loop_add(&sh->loop, shell_loop);
  uv_timer_init(&sh->loop.uv, &sh->loop.delay);

  sh->ptyproc = uv_process_init(&sh->loop, sh);
  sh->proc = &sh->ptyproc.process;
  sh->proc->events = &sh->loop.events;
  sh->proc->in = &sh->in;
  sh->proc->out = &sh->out;
  sh->proc->fin_cb = shell_fin_cb;
  return sh;
}

void shell_free(Shell *sh)
{
  process_teardown(&sh->loop);
  loop_remove(&sh->loop);
  free(sh);
}

void shell_start(Shell *sh, String *args)
{
  log_msg("SHELL", "start");
  sh->proc->argv = args;
  if (sh->blocking) {
    shell_stop(sh);
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
}

void shell_stop(Shell *sh)
{
  log_msg("SHELL", "stop");
  process_stop(sh->proc);
}

void shell_write(Shell *sh, String msg)
{
  log_msg("IMG", "shell_write");
  WBuffer *input_buffer = wstream_new_buffer(msg, strlen(msg), 1, NULL);

  if (!wstream_write(&sh->in, input_buffer)) {
    // couldn't write, stop the process and tell the user about it
    log_msg("IMG", "couldn't write");
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

static void shell_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

static void shell_write_cb(Stream *stream, void *data, int status)
{
  stream_close(stream, NULL);
}
