#include <ncurses.h>

#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/cmd.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"
#include "fnav/event/input.h"
#include "fnav/compl.h"
#include "fnav/tui/fm_cntlr.h"

struct Window {
  Loop loop;
  Layout layout;
  uv_timer_t draw_timer;
  bool ex;
};

Window win;

static void window_loop();
static void* win_new();
static void* win_shut();
static void* win_close();
static void* win_pipe();
static void* win_sort();
static void* win_cd();
static void win_layout();
static void window_ex_cmd();
static void wintest();
static void wintestq();

#define CMDS_SIZE ARRAY_SIZE(cmdtable)
static const Cmd_T cmdtable[] = {
  {"qa",     win_shut,   0},
  {"q",      win_close,   0},
  {"close",  win_close,   0},
  {"new",    win_new,     MOVE_UP},
  {"vnew",   win_new,     MOVE_LEFT},
  {"pipe",   win_pipe,    0},
  {"sort",   win_sort,    1},
  {"sort!",  win_sort,    -1},
  {"cd",     win_cd,      0},
};

#define COMPL_SIZE ARRAY_SIZE(compl_win)
static String compl_win[] = {
  "cmd:q;window:string:wins",
  "cmd:close;window:string:wins",
  "cmd:vnew;cntlr:string:cntlrs",
  "cmd:new;cntlr:string:cntlrs",
  "cmd:pipe;window:number:wins",
  "cmd:sort;field:string:fields",
  "cmd:sort!;field:string:fields",
  "cmd:*:cntlr:fm;path:string:paths",
  "cmd:cd;path:string:paths",
};

#define KEYS_SIZE ARRAY_SIZE(key_defaults)
static fn_key key_defaults[] = {
  {';',     window_ex_cmd,   0,           EX_CMD_STATE},
  {'/',     window_ex_cmd,   0,           EX_REG_STATE},
  {'H',     win_layout,      0,           MOVE_LEFT},
  {'J',     win_layout,      0,           MOVE_DOWN},
  {'K',     win_layout,      0,           MOVE_UP},
  {'L',     win_layout,      0,           MOVE_RIGHT},
  {'s',     wintest,      0,           MOVE_UP},
  {'v',     wintest,      0,           MOVE_LEFT},
  {'q',     wintestq,      0,           MOVE_LEFT},
};
static fn_keytbl key_tbl;
static short cmd_idx[KEYS_SIZE];

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  clear();
  layout_refresh(&win.layout);
  refresh();
}

void window_init(void)
{
  log_msg("INIT", "window");
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = KEYS_SIZE;
  input_setup_tbl(&key_tbl);

  loop_add(&win.loop, window_loop);
  uv_timer_init(&win.loop.uv, &win.loop.delay);
  uv_timer_init(eventloop(), &win.draw_timer);
  win.ex = false;

  signal(SIGWINCH, sig_resize);
  layout_init(&win.layout);

  for (int i = 0; i < (int)CMDS_SIZE; i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    memmove(cmd, &cmdtable[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
  for (int i = 0; i < (int)COMPL_SIZE; i++) {
    compl_add_context(compl_win[i]);
  }
  sig_resize(0);
  buf_setup();
  ex_cmd_init();
}

void window_cleanup(void)
{
  log_msg("CLEANUP", "window_cleanup");
  //buf -> cntlr
  ex_cmd_cleanup();
  cmd_clearall();
  layout_cleanup(&win.layout);
  loop_remove(&win.loop);
}

static void* win_shut()
{
  stop_event_loop();
  return 0;
}

static void win_layout(Window *_w, Cmdarg *arg)
{
  enum move_dir dir = arg->arg;
  layout_movement(&win.layout, dir);
}

static void wintest(Window *_w, Cmdarg *arg)
{
  log_msg("WINDOW", "win_new");
  enum move_dir dir = arg->arg;
  layout_movement(&win.layout, dir);
  window_add_buffer(dir);
}

static void wintestq(Window *_w, Cmdarg *arg)
{
  log_msg("WINDOW", "win_close");
  Buffer *buf = layout_buf(&win.layout);
  //if (buf) {
  //  cntlr_close(buf_cntlr(buf));
  //}
  window_remove_buffer();
}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  if (win.ex) {
    ex_input(key);
  }
  else {
    if (window_get_focus())
      buf_input(layout_buf(&win.layout), key);
    Cmdarg ca;
    int idx = find_command(&key_tbl, key);
    ca.arg = key_defaults[idx].cmd_arg;
    if (idx >= 0) {
      key_defaults[idx].cmd_func(NULL, &ca);
    }
  }
}

Buffer* window_get_focus()
{
  return layout_buf(&win.layout);
}

int window_focus_attached()
{
  return buf_attached(layout_buf(&win.layout));
}

static void* win_pipe(List *args, enum move_dir flags)
{
  log_msg("WINDOW", "win_pipe");
  if (utarray_len(args->items) < 2) return 0;
  Buffer *buf = layout_buf(&win.layout);

  Token *word = (Token*)utarray_eltptr(args->items, 1);
  if (word->var.v_type != VAR_NUMBER) return 0;

  if (buf) {
    int id = TOKEN_NUM(word->var);
    Cntlr *rhs = cntlr_from_id(id);
    //TODO: replace pipe if already set
    send_hook_msg("pipe_attach", buf_cntlr(buf), rhs);
  }
  return 0;
}

static void* win_cd(List *args, int flags)
{
  log_msg("WINDOW", "win_cd");

  String path = "";
  if (utarray_len(args->items) == 2) {
    Token *word = (Token*)utarray_eltptr(args->items, 1);
    if (word->var.v_type != VAR_STRING) return 0;
    path = TOKEN_STR(word->var);
  }
  Cntlr *cntlr = buf_cntlr(layout_buf(&win.layout));
  if (cntlr) {
    fm_req_dir(cntlr, path);
  }
  return 0;
}

static void* win_sort(List *args, int flags)
{
  log_msg("WINDOW", "win_sort");

  String fld = "";
  if (utarray_len(args->items) == 2) {
    Token *word = (Token*)utarray_eltptr(args->items, 1);
    if (word->var.v_type != VAR_STRING) return 0;
    fld = TOKEN_STR(word->var);
  }
  buf_sort(layout_buf(&win.layout), fld, flags);
  return 0;
}

static void* win_new(List *args, enum move_dir flags)
{
  log_msg("WINDOW", "win_new");
  window_add_buffer(flags);

  Cntlr *ret = NULL;
  Token *word = (Token*)utarray_eltptr(args->items, 1);

  if (word) {
    ret = cntlr_open(TOKEN_STR(word->var), layout_buf(&win.layout));
  }
  return ret;
}

static void* win_close(List *args, int cmd_flags)
{
  log_msg("WINDOW", "win_close");
  Buffer *buf = layout_buf(&win.layout);
  if (buf) {
    cntlr_close(buf_cntlr(buf));
  }
  window_remove_buffer();
  return NULL;
}

static void window_ex_cmd(Window *_w, Cmdarg *arg)
{
  win.ex = true;
  start_ex_cmd(arg->arg);
}

void window_ex_cmd_end()
{
  win.ex = false;
}

static void window_loop(Loop *loop, int ms)
{
  if (!queue_empty(&win.loop.events)) {
    process_loop(&win.loop, ms);
    if (win.ex)
      cmdline_refresh();
  }
}

void window_req_draw(void *obj, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(&win.loop.events, cb, 1, obj);
}

void window_remove_buffer()
{
  log_msg("WINDOW", "BUFFER +CLOSE+");
  Buffer *buf = window_get_focus();
  if (buf) {
    buf_delete(buf);
  }
  layout_remove_buffer(&win.layout);
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
