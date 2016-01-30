#include "fnav/lib/utarray.h"

#include "fnav/plugins/term/term.h"
#include "fnav/event/rstream.h"
#include "fnav/event/wstream.h"
#include "fnav/event/event.h"
#include "fnav/event/hook.h"
#include "fnav/tui/buffer.h"
#include "fnav/log.h"
#include "fnav/ascii.h"
#include "fnav/tui/window.h"
#include "fnav/option.h"

#define SCROLL_HISTORY 500

static void readfd_ready(uv_poll_t *, int, int);
static void plugin_resize(Plugin *, Plugin *, void *);
static void plugin_focus(Plugin *);

void term_new(Plugin *plugin, Buffer *buf)
{
  log_msg("TERM", "term_new");
  Term *term = malloc(sizeof(Term));
  term->base = plugin;
  plugin->top = term;
  plugin->name = "term";
  plugin->fmt_name = "VT";
  plugin->_focus = plugin_focus;

  term->win = newwin(1, 1, 0, 0);
  term->buf = buf;
  buf_set_plugin(buf, plugin);
  buf_set_pass(buf);

  term->vt = vt_create(1, 1, SCROLL_HISTORY);
	const char *shell = getenv("SHELL");
	const char *pargs[4] = { shell, NULL };
  char *cwd = window_active_dir();

	vt_forkpty(term->vt, shell, pargs, cwd, NULL, NULL, NULL);

  uv_poll_init(eventloop(), &term->readfd, vt_pty_get(term->vt));
  uv_poll_start(&term->readfd, UV_READABLE, readfd_ready);
  term->readfd.data = term;
  window_start_override(plugin);

  hook_init(plugin);
  hook_add(plugin, plugin, plugin_resize, "window_resize");
}

static void plugin_focus(Plugin *plugin)
{
  Term *term = plugin->top;
  window_start_override(plugin);
  readfd_ready(&term->readfd, 0, 0);
}

void term_delete(Plugin *plugin)
{
}

void term_cursor(Plugin *plugin)
{
  Term *term = plugin->top;
	curs_set(vt_cursor_visible(term->vt));
  wnoutrefresh(term->win);
  doupdate();
}

void term_keypress(Plugin *plugin, int key)
{
  Term *term = plugin->top;
  if (key == Meta('['))
    window_stop_override();
  else if (key == Meta('k')) {
    vt_scroll(term->vt, -1);
    readfd_ready(&term->readfd, 0, 0);
  }
  else if (key == Meta('j')) {
    vt_scroll(term->vt, 1);
    readfd_ready(&term->readfd, 0, 0);
  }
  else
    vt_keypress(term->vt, key);
}

static void term_draw(Term *term)
{
	vt_draw(term->vt, term->win, 0, 0);
  wnoutrefresh(term->win);
  doupdate();
}

static void plugin_resize(Plugin *host, Plugin *none, void *data)
{
  log_msg("TERM", "plugin_resize");
  Term *term = host->top;
  vt_dirty(term->vt);
  wclear(term->win);
  wnoutrefresh(term->win);

  pos_T sz = buf_size(term->buf);
  pos_T ofs = buf_ofs(term->buf);
  wresize(term->win, sz.lnum, sz.col);
  mvwin(term->win, ofs.lnum, ofs.col);
  vt_resize(term->vt, sz.lnum, sz.col);
  term_draw(term);
}

static void readfd_ready(uv_poll_t *handle, int status, int events)
{
  Term *term = handle->data;
  vt_process(term->vt);
  term_draw(term);
}