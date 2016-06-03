#include <sys/wait.h>
#include "nav/lib/utarray.h"

#include "nav/plugins/term/term.h"
#include "nav/event/rstream.h"
#include "nav/event/wstream.h"
#include "nav/event/event.h"
#include "nav/event/hook.h"
#include "nav/event/input.h"
#include "nav/tui/buffer.h"
#include "nav/log.h"
#include "nav/ascii.h"
#include "nav/tui/window.h"
#include "nav/option.h"

#define SCROLL_HISTORY 500

static void readfd_ready(uv_poll_t *, int, int);
static void plugin_resize(Plugin *, Plugin *, HookArg *);
static void plugin_focus(Plugin *);
static void chld_handler(uv_signal_t *, int);
static void plugin_cancel(Plugin *plugin);

void term_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("TERM", "term_new");
  Term *term = malloc(sizeof(Term));
  term->base = plugin;
  term->closed = false;
  term->ed = NULL;
  plugin->top = term;
  plugin->name = "term";
  plugin->fmt_name = "VT";
  plugin->_focus = plugin_focus;
  plugin->_cancel = plugin_cancel;

  term->buf = buf;
  buf_set_plugin(buf, plugin, SCR_NULL);

  term->vt = vt_create(1, 1, SCROLL_HISTORY);
  const char *shell = getenv("SHELL");
  const char *pargs[4] = { shell, NULL };
  char *cwd = window_cur_dir();

  char *args = arg;
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

  hook_add_intl(buf->id, plugin, plugin, plugin_resize, "window_resize");
}

void term_set_editor(Plugin *plugin, Ed *ed)
{
  Term *term = plugin->top;
  term->ed = ed;
}

static void term_cleanup(void **args)
{
  log_msg("TERM", "term_cleanup");
  Term *term = args[0];
  vt_destroy(term->vt);
  free(term);
  term = NULL;
}

void term_delete(Plugin *plugin)
{
  log_msg("TERM", "term_delete");
  Term *term = plugin->top;
  //TODO: close if running
  SLIST_REMOVE(&mainloop()->subterms, term, term, ent);
  uv_poll_stop(&term->readfd);
  window_stop_override();

  uv_handle_t *hp = (uv_handle_t*)&term->readfd;
  uv_cancel((uv_req_t*)&term->readfd);
  uv_close(hp, NULL);
  CREATE_EVENT(eventq(), term_cleanup, 1, term);

  if (term->ed && !term->closed)
    ed_close_cb(term->base, term->ed, true);
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

static void plugin_cancel(Plugin *plugin)
{
  log_msg("TERM", "<|_CANCEL_|>");
  Term *term = plugin->top;
  vt_keypress(term->vt, Ctrl_C);
}

static struct term_key_entry {
  int key;
  char *code;
} termkeytable[] = {
  {K_UP     , "\e[A"},
  {K_DOWN   , "\e[B"},
  {K_RIGHT  , "\e[C"},
  {K_LEFT   , "\e[D"},
  {K_HOME   , "\e[7~"},
  {K_END    , "\e[8~"},
  {K_S_TAB  , "\e[Z"},
  {Ctrl_Z   , "\x1A"},
};

static const char* convert_key(int key)
{
  for (int i = 0; i < LENGTH(termkeytable); i++) {
    if (termkeytable[i].key == key)
      return termkeytable[i].code;
  }
  return "";
}

void term_keypress(Plugin *plugin, Keyarg *ca)
{
  Term *term = plugin->top;
  if (ca->key == Meta('['))
    window_stop_override();
  else if (ca->key == Meta('k')) {
    vt_scroll(term->vt, -1);
    readfd_ready(&term->readfd, 0, 0);
  }
  else if (ca->key == Meta('j')) {
    vt_scroll(term->vt, 1);
    readfd_ready(&term->readfd, 0, 0);
  }
  else {
    if (IS_SPECIAL(ca->key))
      vt_write(term->vt, convert_key(ca->key), 3);
    else
      vt_keypress(term->vt, ca->key);
  }
}

static void term_draw(Term *term)
{
  vt_draw(term->vt, buf_ncwin(term->buf), 0, 0);
  wnoutrefresh(buf_ncwin(term->buf));
  window_update();
  doupdate();
}

static void plugin_resize(Plugin *host, Plugin *none, HookArg *hka)
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
  if (term->ed)
    ed_close_cb(term->base, term->ed, false);
  else
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
