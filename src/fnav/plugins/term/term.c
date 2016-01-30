#include "fnav/lib/utarray.h"

#include "fnav/plugins/term/term.h"
#include "fnav/event/rstream.h"
#include "fnav/event/wstream.h"
#include "fnav/event/event.h"
#include "fnav/event/hook.h"
#include "fnav/tui/buffer.h"
#include "fnav/log.h"
#include "fnav/ascii.h"

static void readfd_ready(uv_poll_t *, int, int);

void term_new(Plugin *plugin, Buffer *buf)
{
  log_msg("TERM", "term_new");
  Term *term = malloc(sizeof(Term));
  term->base = plugin;
  plugin->top = term;

  pos_T sz = buf_size(buf);
  pos_T ofs = buf_size(buf);
  term->win = newwin(sz.lnum, sz.col, 0, 0);
  buf_set_pass(buf);

  term->vt = vt_create(sz.lnum, sz.col, 100);
	const char *shell = "/usr/bin/sh";
	const char *pargs[4] = { shell, NULL };

	vt_forkpty(term->vt, shell, pargs, NULL, NULL, NULL, NULL);

  uv_poll_init(eventloop(), &term->readfd, vt_pty_get(term->vt));
  uv_poll_start(&term->readfd, UV_READABLE, readfd_ready);
  term->readfd.data = term;
}

void term_delete(Plugin *plugin)
{
}

static void readfd_ready(uv_poll_t *handle, int status, int events)
{
  log_msg("TERM", "ready");
  Term *term = handle->data;
  vt_process(term->vt);
	vt_draw(term->vt, term->win, 0, 0);
  wnoutrefresh(term->win);
}
