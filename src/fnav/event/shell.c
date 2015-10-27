// shell

#include <malloc.h>

#include "fnav/event/shell.h"
#include "fnav/model.h"

char* p_sh = "/bin/sh";

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
    bool eof);
static void shell_loop(Loop *loop, int ms);

Shell* shell_init(Cntlr *c, fn_handle *h)
{
  log_msg("SHELL", "init");
  Shell *sh = malloc(sizeof(Shell));
  sh->buf.data = NULL;
  sh->buf.cap = 0;
  sh->buf.len = 0;
  sh->data_cb = out_data_cb;

  loop_add(&sh->loop, shell_loop);
  uv_timer_init(&sh->loop.uv, &sh->loop.delay);

  sh->ptyproc = pty_process_init(&sh->loop, &sh->buf);
  sh->proc = &sh->ptyproc.process;
  sh->proc->events = &sh->loop.events;
  sh->proc->in = &sh->in;
  sh->proc->out = &sh->out;
  sh->proc->err = &sh->err;

  //char *rv[3] = { (char*)p_sh, NULL, NULL };
  char *rv[3] = { "vim", NULL, NULL };
  sh->proc->argv = rv;
  sh->hndl = h;
  sh->out.model = h->model;
  sh->ptyproc.width = 76;
  sh->ptyproc.height = 27;
  return sh;
}

void shell_free(Shell *sh)
{
}

void shell_start(Shell *sh)
{
  log_msg("SHELL", "start");
  if (!process_spawn(sh->proc)) {
    log_msg("PROC", "cannot execute");
    return;
  }
  rstream_init(sh->proc->out, &sh->loop.events, 0);
  rstream_start(sh->proc->out, sh->data_cb);
}

void shell_stop(Shell *sh)
{
  log_msg("SHELL", "stop");
  rstream_stop(sh->proc->out);
}

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
    bool eof)
{
  size_t cnt;
  char *ptr = rbuffer_read_ptr(buf, &cnt);

  if (!cnt) {
    return;
  }

  size_t written = model_read_stream(stream->model, ptr, cnt, false, eof);
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
