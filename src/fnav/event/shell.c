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

Shell* shell_init(Cntlr *c)
{
  log_msg("SHELL", "init");
  Shell *sh = malloc(sizeof(Shell));
  sh->buf.data = NULL;
  sh->buf.cap = 0;
  sh->buf.len = 0;
  sh->data_cb = out_data_cb;

  loop_add(&sh->loop, shell_loop);
  uv_timer_init(&sh->loop.uv, &sh->loop.delay);

  sh->ptyproc = uv_process_init(&sh->loop, NULL);
  sh->proc = &sh->ptyproc.process;
  sh->proc->events = &sh->loop.events;
  sh->proc->in = &sh->in;
  sh->proc->out = &sh->out;

  return sh;
}

void shell_free(Shell *sh)
{
}

void shell_start(Shell *sh, String *args)
{
  log_msg("SHELL", "start");
  sh->proc->argv = args;
  if (!process_spawn(sh->proc)) {
    log_msg("PROC", "cannot execute");
    return;
  }
  sh->proc->in->events = NULL;
  wstream_init(sh->proc->in, 0);
  rstream_init(sh->proc->out, &sh->loop.events, 0);
  rstream_start(sh->proc->out, sh->data_cb);
}

void shell_stop(Shell *sh)
{
  log_msg("SHELL", "stop");
  rstream_stop(sh->proc->out);
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
  size_t cnt;
  char *ptr = rbuffer_read_ptr(buf, &cnt);

  if (!cnt) {
    return;
  }

  log_msg("SHELL", "out_data_cb");
  log_msg("SHELL", "%s", ptr);
  //size_t written = model_read_stream(stream->model, ptr, cnt, false, eof);
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
