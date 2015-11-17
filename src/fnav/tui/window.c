// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/cmd.h"
#include "fnav/log.h"

struct Window {
  Loop loop;
  Layout layout;
  Layout root;
  uv_timer_t draw_timer;
  bool ex;
};

Window win;

static void window_loop();
static void* win_new();
static void* win_close();

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  clear();
  layout_refresh(&win.root);
}

#define CMDS_SIZE ARRAY_SIZE(cmdtable)

static const Cmd_T cmdtable[] = {
  {"q",      win_close,   0 },
  {"clo",    win_close,   0 },
  {"close",  win_close,   0 },
  {"new",    win_new,     MOVE_UP},
  {"vnew",   win_new,     MOVE_LEFT},
  {"vne",    win_new,     MOVE_LEFT},
};

void window_init(void)
{
  log_msg("INIT", "window");
  loop_add(&win.loop, window_loop);
  uv_timer_init(&win.loop.uv, &win.loop.delay);
  uv_timer_init(eventloop(), &win.draw_timer);
  win.ex = false;

  signal(SIGWINCH, sig_resize);
  layout_init(&win.layout);
  win.root = win.layout;

  for (int i = 0; i < CMDS_SIZE; i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memcpy(cmd, &cmdtable[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  if (win.ex) {
    ex_input(key);
  }
  else {
    if (key == ';') {
      window_ex_cmd_start(EX_CMD_STATE);
    }
    if (key == '/') {
      window_ex_cmd_start(EX_REG_STATE);
    }
    if (key == 'H')
      layout_movement(&win.layout, MOVE_LEFT);
    if (key == 'J')
      layout_movement(&win.layout, MOVE_DOWN);
    if (key == 'K')
      layout_movement(&win.layout, MOVE_UP);
    if (key == 'L')
      layout_movement(&win.layout, MOVE_RIGHT);
    if (window_get_focus())
      buf_input(layout_buf(&win.layout), key);
  }
}

Buffer* window_get_focus()
{
  return layout_buf(&win.layout);
}

static void* win_new(List *args, enum move_dir flags)
{
  log_msg("WINDOW", "win_new");
  window_add_buffer(flags);
  // TODO: replace with cntlr name lookup.
  //       load. init here. add to cntlr list etc.
  //       open. create instance.
  //       close. close instance.
  //       unload. cleanup here. remove from cntlr list.
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

void window_ex_cmd_start(int state)
{
  win.ex = true;
  start_ex_cmd(state);
}

void window_ex_cmd_end()
{
  win.ex = false;
}

static void window_loop(Loop *loop, int ms)
{
  if (!queue_empty(&win.loop.events)) {
    process_loop(&win.loop, ms);
    doupdate();
    if (win.ex)
      cmdline_refresh();
  }
}

void window_req_draw(void *obj, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(&win.loop.events, cb, 1, obj);
}

void window_set_status(String label)
{
  layout_set_status(&win.layout, label);
}

void window_draw_all()
{
  log_msg("WINDOW", "DRAW_ALL");
}

void window_remove_buffer()
{
  log_msg("WINDOW", "BUFFER +CLOSE+");
  //buf_cleanup(bn->buf);
  //free(bn);
  layout_remove_buffer(&win.layout);
  window_draw_all();
}

void window_add_buffer(enum move_dir dir)
{
  log_msg("WINDOW", "BUFFER +ADD+");
  Buffer *buf = buf_init();
  layout_add_buffer(&win.layout, buf, dir);
}
