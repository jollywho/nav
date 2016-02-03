#include <sys/wait.h>

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
static void chld_handler(uv_signal_t *, int);

void term_new(Plugin *plugin, Buffer *buf, void *arg)
{
  log_msg("TERM", "term_new");
  Term *term = malloc(sizeof(Term));
  term->base = plugin;
  term->closed = false;
  plugin->top = term;
  plugin->name = "term";
  plugin->fmt_name = "VT";
  plugin->_focus = plugin_focus;

  term->buf = buf;
  buf_set_plugin(buf, plugin);
  buf_set_pass(buf);

  term->vt = vt_create(1, 1, SCROLL_HISTORY);
  const char *shell = getenv("SHELL");
  const char *pargs[4] = { shell, NULL };
  char *cwd = window_cur_dir();

  String args = arg;
  if (args && args[0]) {
    pargs[1] = "-c";
    pargs[2] = args;
    pargs[3] = NULL;
  }

  SLIST_INSERT_HEAD(&mainloop()->subterms, term, ent);
  uv_signal_start(&mainloop()->children_watcher, chld_handler, SIGCHLD);

  term->pid = vt_forkpty(term->vt, shell, pargs, cwd, NULL, NULL, NULL);
  if (term->closed) {
    return;
  }

  uv_poll_init(eventloop(), &term->readfd, vt_pty_get(term->vt));
  uv_poll_start(&term->readfd, UV_READABLE, readfd_ready);
  term->readfd.data = term;
  window_start_override(plugin);

  hook_init(plugin);
  hook_add(plugin, plugin, plugin_resize, "window_resize");
}

static void term_close_poll(uv_handle_t *hndl)
{
  Term *term = hndl->data;
  vt_destroy(term->vt);
  free(term);
}

void term_delete(Plugin *plugin)
{
  log_msg("TERM", "term_delete");
  Term *term = plugin->top;
  //TODO: close if running
  SLIST_REMOVE(&mainloop()->subterms, term, term, ent);
  uv_poll_stop(&term->readfd);
  window_stop_override();
  hook_cleanup(term->base);
  uv_handle_t *hp = (uv_handle_t*)&term->readfd;
  uv_close(hp, term_close_poll);
}

static void plugin_focus(Plugin *plugin)
{
  Term *term = plugin->top;
  window_start_override(plugin);
  readfd_ready(&term->readfd, 0, 0);
}

void term_cursor(Plugin *plugin)
{
  Term *term = plugin->top;
  curs_set(vt_cursor_visible(term->vt));
  wnoutrefresh(buf_ncwin(term->buf));
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
  vt_draw(term->vt, buf_ncwin(term->buf), 0, 0);
  wnoutrefresh(buf_ncwin(term->buf));
  doupdate();
}

static void plugin_resize(Plugin *host, Plugin *none, void *data)
{
  log_msg("TERM", "plugin_resize");
  Term *term = host->top;
  vt_dirty(term->vt);

  pos_T sz = buf_size(term->buf);
  pos_T ofs = buf_ofs(term->buf);
  wresize(buf_ncwin(term->buf), sz.lnum, sz.col);
  mvwin(buf_ncwin(term->buf), ofs.lnum, ofs.col);
  vt_resize(term->vt, sz.lnum, sz.col);
  term_draw(term);
}

static void readfd_ready(uv_poll_t *handle, int status, int events)
{
  Term *term = handle->data;
  vt_process(term->vt);
  term_draw(term);
}

static void term_close(Term *term)
{
  log_msg("TERM", "term_close");
  term->closed = true;
  window_close_focus();
}

static void chld_handler(uv_signal_t *handle, int signum)
{
  int stat = 0;
  int pid;

  do {
    pid = waitpid(-1, &stat, WNOHANG);
  } while (pid < 0 && errno == EINTR);

  if (pid <= 0)
    return;

  Term *it;
  SLIST_FOREACH(it, &mainloop()->subterms, ent) {
    Term *term = it;
    if (term->pid == pid) {
      if (WIFEXITED(stat))
        term->status = WEXITSTATUS(stat);
      else if (WIFSIGNALED(stat))
        term->status = WTERMSIG(stat);

      term_close(it);
      break;
    }
  }
}
