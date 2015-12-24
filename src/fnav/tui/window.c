// window module to manage draw loop.
// all ncurses drawing should be done here.
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

struct Window {
  Loop loop;
  Layout layout;
  uv_timer_t draw_timer;
  bool ex;
};

Window win;

static void window_loop();
static void* win_new();
static void* win_close();
static void* win_pipe();
static void win_layout();
static void window_ex_cmd();

#define CMDS_SIZE ARRAY_SIZE(cmdtable)
static const Cmd_T cmdtable[] = {
  {"q",      win_close,   0 },
  {"clo",    win_close,   0 },
  {"close",  win_close,   0 },
  {"new",    win_new,     MOVE_UP},
  {"vnew",   win_new,     MOVE_LEFT},
  {"vne",    win_new,     MOVE_LEFT},
  {"pipe",   win_pipe,    0 },
};

#define KEYS_SIZE ARRAY_SIZE(key_defaults)
static fn_key key_defaults[] = {
  {';',     window_ex_cmd,   0,           EX_CMD_STATE},
  {'/',     window_ex_cmd,   0,           EX_REG_STATE},
  {'H',     win_layout,      0,           MOVE_LEFT},
  {'J',     win_layout,      0,           MOVE_DOWN},
  {'K',     win_layout,      0,           MOVE_UP},
  {'L',     win_layout,      0,           MOVE_RIGHT},
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
  input_init_tbl(&key_tbl);

  loop_add(&win.loop, window_loop);
  uv_timer_init(&win.loop.uv, &win.loop.delay);
  uv_timer_init(eventloop(), &win.draw_timer);
  win.ex = false;

  signal(SIGWINCH, sig_resize);
  layout_init(&win.layout);

  for (int i = 0; i < (int)CMDS_SIZE; i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memcpy(cmd, &cmdtable[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
  sig_resize(0);
}

static void win_layout(Window *_w, Cmdarg *arg)
{
  enum move_dir dir = arg->arg;
  layout_movement(&win.layout, dir);
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
    send_hook_msg("pipe_attach", buf_cntlr(buf), rhs );
  }
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

void window_draw_all()
{
  log_msg("WINDOW", "DRAW_ALL");
}

void window_remove_buffer()
{
  log_msg("WINDOW", "BUFFER +CLOSE+");
  Buffer *buf = window_get_focus();
  if (buf) {
    buf_delete(buf);
  }
  layout_remove_buffer(&win.layout);
  window_draw_all();
}

void window_add_buffer(enum move_dir dir)
{
  log_msg("WINDOW", "BUFFER +ADD+");
  Buffer *buf = buf_new();
  layout_add_buffer(&win.layout, buf, dir);
}
