#include <ncurses.h>

#include "nav/tui/window.h"
#include "nav/event/event.h"
#include "nav/tui/layout.h"
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
  Keyarg ca;
  bool ex;
  bool dirty;
  int refs;
  bool input_override;
  Plugin *term;
};

static void* win_new();
static void* win_shut();
static void* win_close();
static void* win_sort();
static void* win_buf();
static void* win_bdel();
static void* win_cd();
static void* win_mark();
static void* win_autocmd();
static void* win_echo();
static void* win_reload();
static void win_layout();
static void window_ex_cmd();
static void window_update(uv_timer_t *);

static const Cmd_T cmdtable[] = {
  {"qa",0,          win_shut,    0},
  {"close","q",     win_close,   0},
  {"autocmd","au",  win_autocmd, 0},
  {"buffer","bu",   win_buf,     0},
  {"bdelete","bd",  win_bdel,    0},
  {"new",0,         win_new,     MOVE_UP},
  {"vnew","vne",    win_new,     MOVE_LEFT},
  {"sort","sor",    win_sort,    1},
  {"cd",0,          win_cd,      0},
  {"mark",0,        win_mark,    0},
  {"delm",0,        win_mark,    1},
  {"echo","ec",     win_echo,    0},
  {"reload","rel",  win_reload,  0},
};

static char *compl_cmds[] = {
  "q;window:string:wins",
  "close;window:string:wins",
  "au;event:string:events",
  "autocmd;event:string:events",
  "bu;plugin:string:plugins",
  "buffer;plugin:string:plugins",
  "bd;window:string:wins",
  "bdelete;window:string:wins",
  "vnew;plugin:string:plugins",
  "new;plugin:string:plugins",
  "sort;field:string:fields",
  "cd;path:string:paths",
  "mark;label:string:marklbls",
  "set;option:string:options",
  "op;group:string:groups",
};
static char *compl_args[][2] = {
  {"plugin", "fm;path:string:paths"},
  {"plugin", "img;window:number:wins"},
  {"plugin", "dt;path:string:paths"},
};

static fn_key key_defaults[] = {
  {':',     window_ex_cmd,   0,           EX_CMD_STATE},
  {'/',     window_ex_cmd,   0,           EX_REG_STATE},
  {'H',     win_layout,      0,           MOVE_LEFT},
  {'J',     win_layout,      0,           MOVE_DOWN},
  {'K',     win_layout,      0,           MOVE_UP},
  {'L',     win_layout,      0,           MOVE_RIGHT},
};

static fn_keytbl key_tbl;
static short cmd_idx[LENGTH(key_defaults)];
static const uint64_t RFSH_RATE = 10;
static Window win;
static char *cur_dir;

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  clear();
  layout_refresh(&win.layout);
  refresh();
  event_wakeup();
  window_update(&win.draw_timer);
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

  signal(SIGWINCH, sig_resize);
  layout_init(&win.layout);

  for (int i = 0; i < LENGTH(cmdtable); i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    memmove(cmd, &cmdtable[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
  for (int i = 0; i < LENGTH(compl_cmds); i++)
    compl_add_context(compl_cmds[i]);
  for (int i = 0; i < LENGTH(compl_args); i++)
    compl_add_arg(compl_args[i][0], compl_args[i][1]);

  curs_set(0);
  sig_resize(0);
  plugin_init();
  buf_init();
}

void window_cleanup(void)
{
  log_msg("CLEANUP", "window_cleanup");
  cmd_clearall();
  layout_cleanup(&win.layout);
  buf_cleanup();
}

static void* win_shut()
{
  uv_timer_stop(&win.draw_timer);
  stop_event_loop();
  return 0;
}

static void win_layout(Window *_w, Keyarg *arg)
{
  enum move_dir dir = arg->arg;
  layout_movement(&win.layout, dir);
}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  win.ca.key = key;
  if (win.input_override)
    return term_keypress(win.term, key);
  if (win.ex)
    return ex_input(key);

  if (input_map_exists(key))
    return do_map(key);

  int ret = 0;
  if (window_get_focus())
    ret = buf_input(layout_buf(&win.layout), &win.ca);
  if (!ret)
    find_do_cmd(&key_tbl, &win.ca, NULL);
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
  return window_get_focus()->plugin;
}

int window_focus_attached()
{
  if (!layout_buf(&win.layout))
    return 0;
  return buf_attached(layout_buf(&win.layout));
}

static void* win_cd(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_cd");
  Plugin *plugin = buf_plugin(layout_buf(&win.layout));
  char *path = cmdline_line_from(ca->cmdline, 1);
  if (plugin)
    send_hook_msg("open", plugin, NULL, &(HookArg){NULL,path});
  //TODO: store curdir separate from FM. empty plugin bufs need curdirs.
  return 0;
}

static void* win_mark(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_mark");

  char *label = list_arg(args, 1, VAR_STRING);
  if (!label)
    return 0;

  Plugin *plugin = buf_plugin(layout_buf(&win.layout));
  if (plugin)
    mark_label_dir(label, window_cur_dir());
  return 0;
}

static void* win_autocmd(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_autocmd");
  int len = utarray_len(args->items);
  log_msg("WINDOW", "%d %d", len, ca->cmdstr->rev);
  char *event = list_arg(args, 1, VAR_STRING);
  int pos = len > 3 ? 2 : -1;
  int rem = len > 3 ? 3 : 2;
  char *pat = list_arg(args, pos, VAR_STRING);
  char *cur = cmdline_line_from(ca->cmdline, rem);

  if (event && ca->cmdstr->rev)
    hook_remove(event, pat);
  else if (event && cur)
    hook_add(event, pat, cur);
  return 0;
}

static void* win_echo(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_echo");
  //TODO: print from cmdstr, not tokens or raw
  char *out = cmdline_line_from(ca->cmdline, 1);
  log_msg(">", "%s", out);
  return out;
}

static void* win_sort(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_sort");
  char *fld = list_arg(args, 1, VAR_STRING);
  buf_sort(layout_buf(&win.layout), fld, ca->cmdstr->rev);
  return 0;
}

static void* win_reload(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_reload");
  Plugin *plugin = buf_plugin(layout_buf(&win.layout));
  if (plugin)
    send_hook_msg("open", plugin, NULL, &(HookArg){NULL,window_cur_dir()});
  return 0;
}

static void* win_new(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_new");
  char *name = list_arg(args, 1, VAR_STRING);
  if (!name)
    window_add_buffer(ca->flags);
  if (!plugin_requires_buf(name))
    return 0;

  if (!(ca->pflag & BUFFER))
    window_add_buffer(ca->flags);
  Token *cmd = tok_arg(args, 2);
  char *path = NULL;
  if (cmd)
    path = ca->cmdline->line + cmd->start;
  return plugin_open(name, layout_buf(&win.layout), path);
}

static void* win_close(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_close");
  Buffer *buf = layout_buf(&win.layout);
  if (buf)
    plugin_close(buf_plugin(buf));

  if (!(ca->pflag & BUFFER) && buf)
    window_remove_buffer();
  else if (buf)
    buf_detach(buf);
  return NULL;
}

static void* win_buf(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_buf");
  char *name = list_arg(args, 1, VAR_STRING);
  if (!name || layout_is_root(&win.layout))
    return 0;

  ca->pflag = BUFFER;
  win_close(args, ca);
  win_new(args, ca);
  return 0;
}

static void* win_bdel(List *args, Cmdarg *ca)
{
  log_msg("WINDOW", "win_bdel");
  ca->pflag = BUFFER;
  win_close(args, ca);
  return 0;
}

void window_close_focus()
{
  win_close(NULL, &(Cmdarg){});
}

static void window_ex_cmd(Window *_w, Keyarg *arg)
{
  if (!window_focus_attached() && arg->arg == EX_REG_STATE)
    return;
  win.ex = true;
  start_ex_cmd(arg->arg);
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

void window_remove_buffer()
{
  log_msg("WINDOW", "BUFFER +CLOSE+");
  Buffer *buf = window_get_focus();
  if (buf)
    buf_delete(buf);

  layout_remove_buffer(&win.layout);

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

void window_shift(int lines)
{
  layout_shift(&win.layout, lines);
}
