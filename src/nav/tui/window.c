#include "nav/tui/window.h"
#include "nav/event/event.h"
#include "nav/tui/layout.h"
#include "nav/tui/message.h"
#include "nav/cmd.h"
#include "nav/compl.h"
#include "nav/event/hook.h"
#include "nav/event/input.h"
#include "nav/info.h"
#include "nav/log.h"
#include "nav/tui/ex_cmd.h"
#include "nav/plugins/term/term.h"
#include "nav/event/fs.h"

struct Window {
  Layout layout;
  uv_timer_t draw_timer;
  bool ex;
  bool dirty;
  int refs;
  bool input_override;
  Plugin *term;
};

static Cmdret win_version();
static Cmdret win_new();
static Cmdret win_shut();
static Cmdret win_close();
static Cmdret win_sort();
static Cmdret win_buf();
static Cmdret win_bdel();
static Cmdret win_cd();
static Cmdret win_mark();
static Cmdret win_echo();
static Cmdret win_reload();
static Cmdret win_direct();
static Cmdret win_edit();
static Cmdret win_filter();
static void win_layout();
static void window_ex_cmd();
static void window_reg_cmd();
static void window_fltr_cmd();
static void window_update(uv_timer_t *);

static const Cmd_T cmdtable[] = {
  {"bdelete","bd",   "close a buffer",         win_bdel,      0},
  {"buffer","bu",    "change a buffer",        win_buf,       0},
  {"cd",0,           "change directory",       win_cd,        0},
  {"close","q",      "close a window",         win_close,     0},
  {"delmark","delm", "delete a mark",          win_mark,      1},
  {"direct","di",    "direct at window",       win_direct,    0},
  {"echo","ec",      "",                       win_echo,      0},
  {"edit","ed",      "",                       win_edit,      0},
  {"filter","fil",   "",                       win_filter,    0},
  {"mark","m",       "mark a directory",       win_mark,      0},
  {"new",0,          "open horizontal window", win_new,       MOVE_UP},
  {"qa",0,           "quit all",               win_shut,      0},
  {"reload","rel",   "set option value",       win_reload,    0},
  {"sort","sor",     "sort lines",             win_sort,      1},
  {"version","ver",  "",                       win_version,   0},
  {"vnew","vne",     "open vertical window",   win_new,       MOVE_LEFT},
};

static fn_key key_defaults[] = {
  {':',     window_ex_cmd,   0,           EX_CMD_STATE},
  {'?',     window_reg_cmd,  0,           BACKWARD},
  {'/',     window_reg_cmd,  0,           FORWARD},
  {'H',     win_layout,      0,           MOVE_LEFT},
  {'J',     win_layout,      0,           MOVE_DOWN},
  {'K',     win_layout,      0,           MOVE_UP},
  {'L',     win_layout,      0,           MOVE_RIGHT},
  {Ctrl_W,  oper,            NCH_A,       OP_WIN},
  {'f',     window_fltr_cmd, 0,           FORWARD},
};

static fn_keytbl key_tbl;
static short cmd_idx[LENGTH(key_defaults)];
static const uint64_t RFSH_RATE = 10;
static Window win;
static char *cur_dir;

void sigwinch_handler(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  if (sig == SIGWINCH)
    endwin();
  refresh();
  clear();
  window_refresh();
  refresh();
  event_wakeup();
  window_update(&win.draw_timer);
}

void sigint_handler(int sig)
{
  log_msg("WINDOW", "Signal received: **sig int**");
  Plugin *plugin = window_get_plugin();
  if (plugin && plugin->_cancel)
    plugin->_cancel(plugin);
}

void window_init(void)
{
  log_msg("INIT", "window");
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = LENGTH(key_defaults);
  input_setup_tbl(&key_tbl);

  uv_timer_init(eventloop(), &win.draw_timer);

  cur_dir = fs_current_dir();
  win.ex = false;
  win.dirty = false;
  win.refs = 0;

  signal(SIGINT,   sigint_handler);
  signal(SIGWINCH, sigwinch_handler);
  layout_init(&win.layout);

  for (int i = 0; i < LENGTH(cmdtable); i++)
    cmd_add(&cmdtable[i]);

  sigwinch_handler(~SIGWINCH); //flush window without endwin
  plugin_init();
  buf_init();
}

void window_cleanup(void)
{
  log_msg("CLEANUP", "window_cleanup");
  layout_cleanup(&win.layout);
  buf_cleanup();
  plugin_cleanup();
}

void window_refresh()
{
  cmdline_resize();
  layout_refresh(&win.layout, ex_cmd_height());
}

static Cmdret win_shut()
{
  uv_timer_stop(&win.draw_timer);
  stop_event_loop();
  return NORET;
}

static void win_layout(Window *_w, Keyarg *ca)
{
  enum move_dir dir = ca->arg;
  layout_movement(&win.layout, dir);
}

void window_input(Keyarg *ca)
{
  log_msg("WINDOW", "input");

  if (message_pending)
    msg_clear();

  if (dialog_pending)
    return dialog_input(ca->key);
  if (win.input_override)
    return term_keypress(win.term, ca);
  if (win.ex)
    return ex_input(ca);

  bool ret = try_do_map(ca);
  if (!ret && window_get_focus())
    ret = buf_input(layout_buf(&win.layout), ca);
  if (!ret)
    ret = find_do_key(&key_tbl, ca, &win);
}

void window_start_override(Plugin *term)
{
  win.input_override = true;
  win.term = term;
}

void window_stop_override()
{
  log_msg("WINDOW", "window_stop_term");
  curs_set(0);
  win.input_override = false;
  win.term = NULL;
}

void window_ch_dir(char *dir)
{
  SWAP_ALLOC_PTR(cur_dir, strdup(dir));
}

char* window_cur_dir()
{
  return cur_dir;
}

Buffer* window_get_focus()
{
  return layout_buf(&win.layout);
}

Plugin* window_get_plugin()
{
  Buffer *buf = window_get_focus();
  if (!buf)
    return NULL;
  return buf->plugin;
}

int window_focus_attached()
{
  if (!layout_buf(&win.layout))
    return 0;
  return buf_attached(layout_buf(&win.layout));
}

Cmdret win_cd(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_cd");
  Plugin *plugin = NULL;
  Buffer *buf = layout_buf(&win.layout);
  char *path = cmdline_line_from(ca->cmdline, 1);

  if (buf)
    plugin = buf_plugin(buf);

  if (!plugin) {
    char *newpath = valid_full_path(window_cur_dir(), path);
    if (newpath) {
      window_ch_dir(newpath);
      free(newpath);
    }
    else {
      nv_err("not a valid path: %s", path);
      return NORET;
    }
  }

  if (plugin)
    send_hook_msg("open", plugin, NULL, &(HookArg){NULL,path});

  return NORET;
}

Cmdret win_mark(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_mark");

  char *label = list_arg(args, 1, VAR_STRING);
  if (!label)
    return NORET;

  Plugin *plugin = buf_plugin(layout_buf(&win.layout));
  if (plugin)
    mark_label_dir(label, window_cur_dir());
  return NORET;
}

Cmdret win_echo(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_echo");
  char *out = cmdline_line_from(ca->cmdline, 1);
  if (!out)
    return NORET;
  return (Cmdret){OUTPUT, .val.v_str = out};
}

Cmdret win_sort(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_sort");
  char *fld = list_arg(args, 1, VAR_STRING);
  if (!fld)
    return NORET;
  buf_sort(layout_buf(&win.layout), fld, ca->cmdstr->rev);
  return NORET;
}

Cmdret win_reload(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_reload");
  char *path = window_cur_dir();
  fs_clr_cache(path);
  fs_reload(path);
  return NORET;
}

Cmdret win_version(List *args, Cmdarg *ca)
{
  log_msg("-", "%s", NAV_LONG_VERSION);
  return (Cmdret){OUTPUT, .val.v_str = NAV_LONG_VERSION};
}

Cmdret win_new(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_new");
  char *name = list_arg(args, 1, VAR_STRING);
  if (!name)
    window_add_buffer(ca->flags);
  if (!plugin_requires_buf(name))
    return NORET;

  window_add_buffer(ca->flags);

  char *path_arg = cmdline_line_from(ca->cmdline, 2);
  int id = plugin_open(name, layout_buf(&win.layout), path_arg);
  return (Cmdret){RET_INT, .val.v_int = id};
}

Cmdret win_close(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_close");

  Buffer *buf = layout_buf(&win.layout);
  if (!buf)
    return win_shut();

  Plugin *plug = buf_plugin(buf);

  if (utarray_len(args->items) > 1) {
    char *arg = list_arg(args, 1, VAR_STRING);
    int wnum;
    if (!str_num(arg, &wnum))
      return NORET;
    buf = buf_from_id(wnum);
    if (!buf) {
      nv_err("invalid buffer %s", arg);
      return NORET;
    }
    plug = buf_plugin(buf);
  }

  if (plug)
    plugin_close(plug);
  window_remove_buffer(buf);
  return NORET;
}

Cmdret win_buf(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_buf");
  Buffer *buf = window_get_focus();
  Plugin *plug = buf_plugin(buf);

  int argidx = 1;
  char *arg = list_arg(args, argidx, VAR_STRING);
  int wnum;
  if (str_num(arg, &wnum)) {
    buf = buf_from_id(wnum);
    if (!buf) {
      nv_err("invalid buffer %s", arg);
      return NORET;
    }
    argidx++;
    plug = buf_plugin(buf);
  }

  char *name = list_arg(args, argidx, VAR_STRING);
  if (!name || !buf)
    return NORET;

  if (plug)
    plugin_close(plug);
  buf_detach(buf);

  char *path_arg = cmdline_line_from(ca->cmdline, argidx+1);
  int id = plugin_open(name, buf, path_arg);
  return (Cmdret){RET_INT, .val.v_int = id};
}

Cmdret win_bdel(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_bdel");
  Buffer *buf = window_get_focus();
  Plugin *plug = buf_plugin(buf);
  if (!plug)
    return NORET;

  int argidx = 1;
  char *arg = list_arg(args, argidx, VAR_STRING);
  int wnum;
  if (str_num(arg, &wnum)) {
    buf = buf_from_id(wnum);
    if (!buf) {
      nv_err("invalid buffer %s", arg);
      return NORET;
    }
    argidx++;
    plug = buf_plugin(buf);
  }
  plugin_close(plug);
  buf_detach(buf);
  return NORET;
}

Cmdret win_direct(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_direct");
  if (utarray_len(args->items) < 1)
    return NORET;
  char *arg = list_arg(args, 1, VAR_STRING);
  int wnum;
  if (!str_num(arg, &wnum))
    return NORET;

  Plugin *lhs = focus_plugin();
  Plugin *rhs = plugin_from_id(wnum);
  if (!rhs)
    return NORET;

  log_msg("WINDOW", "%d", wnum);
  if (ca->cmdstr->rev)
    send_hook_msg("pipe_remove", lhs, rhs, NULL);
  else
    send_hook_msg("pipe_left", lhs, rhs, NULL);
  return NORET;
}

Cmdret win_edit(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_edit");
  Buffer *buf = window_get_focus();
  if (!buf)
    return NORET;

  int id = buf_id(buf);
  char *line;
  asprintf(&line, "new ed %d", id);
  cmd_eval(NULL, line);
  free(line);
  buf_end_sel(buf);

  return NORET;
}

Cmdret win_filter(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_filter");
  return NORET;
}

void win_move(void *_w, Keyarg *ca)
{
  log_msg("WINDOW", "win_move");
  int dir = key2move_type(ca->key);
  if (dir < 0)
    return;
  layout_swap(&win.layout, dir);
}

void window_close_focus()
{
  Buffer *buf = layout_buf(&win.layout);
  Plugin *plug = buf_plugin(buf);
  plugin_close(plug);
  window_remove_buffer(buf);
}

static void window_ex_cmd(Window *_w, Keyarg *arg)
{
  if (!window_focus_attached() && arg->arg != EX_CMD_STATE)
    return;
  win.ex = true;
  start_ex_cmd(arg->key, arg->arg);
}

static void window_reg_cmd(Window *_w, Keyarg *arg)
{
  regex_setsign(arg->arg);
  arg->arg = EX_REG_STATE;
  window_ex_cmd(_w, arg);
}

static void window_fltr_cmd(Window *_w, Keyarg *arg)
{
  arg->arg = EX_FIL_STATE;
  arg->key = '*';
  window_ex_cmd(_w, arg);
}

void window_ex_cmd_end()
{
  win.ex = false;
}

void window_draw(void **argv)
{
  win.refs--;

  void *obj = argv[0];
  argv_callback cb = argv[1];
  cb(&obj);
}

static void window_update(uv_timer_t *handle)
{
  log_msg("WINDOW", "win update");
  if (win.ex)
    cmdline_refresh();
  if (win.input_override)
    term_cursor(win.term);

  doupdate();

  if (win.refs < 1) {
    uv_timer_stop(handle);
    win.dirty = false;
  }
}

void window_req_draw(void *obj, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  if (!win.dirty) {
    win.dirty = true;
    uv_timer_start(&win.draw_timer, window_update, RFSH_RATE, RFSH_RATE);
  }
  if (obj && cb) {
    win.refs++;
    CREATE_EVENT(drawq(), window_draw, 2, obj, cb);
  }
}

void window_remove_buffer(Buffer *buf)
{
  log_msg("WINDOW", "BUFFER +CLOSE+");
  if (buf)
    buf_delete(buf);

  layout_remove_buffer(&win.layout, buf);

  if (layout_is_root(&win.layout)) {
    clear();
    refresh();
  }
}

void window_add_buffer(enum move_dir dir)
{
  log_msg("WINDOW", "BUFFER +ADD+");
  Buffer *buf = buf_new();
  layout_add_buffer(&win.layout, buf, dir);
}
