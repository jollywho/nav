#include "fnav/lib/utarray.h"

#include "fnav/plugins/term/term.h"
#include "fnav/event/rstream.h"
#include "fnav/event/wstream.h"
#include "fnav/event/event.h"
#include "fnav/log.h"

char *argv[2] = {"/bin/sh", NULL};
static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
  bool eof);

void term_new(Plugin *plugin, Buffer *buf)
{
  log_msg("TERM", "term_new");
  Term *term = malloc(sizeof(Term));
  term->base = plugin;
  plugin->top = term;
  term->pty = pty_process_init(mainloop(), 0);
  Process *proc = &term->pty.process;
  proc->argv = argv;
  proc->events = eventq();

  proc->in = &term->in;
  proc->out = &term->out;
  proc->err = &term->err;

  process_spawn(proc);

  wstream_init(&term->in, 0);
  rstream_init(&term->out, 0);
  rstream_start(&term->out, out_data_cb);
}

void term_delete(Plugin *plugin)
{
}

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
  bool eof)
{
  log_msg("TERM", "out_data_cb");
  size_t cnt;
  char *ptr = rbuffer_read_ptr(buf, &cnt);

  if (!cnt) {
    return;
  }

  ptr[count] = '\0';
  log_msg("TERM", "ptr: %s", ptr);

  size_t written = count;
  // No output written, force emptying the Rbuffer if it is full.
  if (!written && rbuffer_size(buf) == rbuffer_capacity(buf)) {
    written = cnt;
  }
  if (written) {
    rbuffer_consumed(buf, count);
  }
}

static void term_write_cb(Stream *stream, void *data, int status)
{
  stream_close(stream, NULL);
}

void term_write(Term *term, String msg)
{
  log_msg("TERM", "write_to_stream");
  WBuffer *input_buffer = wstream_new_buffer(msg, strlen(msg), 1, NULL);
  Process *proc = &term->pty.process;

  if (!wstream_write(proc->in, input_buffer)) {
    // couldn't write, stop the process and tell the user about it
    log_msg("TERM", "couldn't write");
    process_stop(proc);
    return;
  }
  // close the input stream after everything is written
  wstream_set_write_cb(proc->in, term_write_cb);
}
