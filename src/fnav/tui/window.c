// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/cmd.h"

struct Window {
  Loop loop;
  BufferNode *focus;
  uv_timer_t draw_timer;
  int buf_count;
  bool ex;
};

Window win;

static void window_loop(Loop *loop, int ms);
static void win_new(Token *arg, int cmd_flags);
static void win_close(Token *arg, int cmd_flags);

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  // TODO: redo layout
}

#define DIR_HORZ  0
#define DIR_VERT  1

#define CMDS_SIZE ARRAY_SIZE(cmdtable)

static const Cmd_T cmdtable[] = {
  {"q",      win_close,   0},
  {"new",    win_new,     DIR_HORZ},
  {"vnew",   win_new,     DIR_VERT},
  {"vne",    win_new,     DIR_VERT},
};

void window_init(void)
{
  log_msg("INIT", "window");
  loop_add(&win.loop, window_loop);
  uv_timer_init(&win.loop.uv, &win.loop.delay);
  uv_timer_init(eventloop(), &win.draw_timer);
  win.focus = NULL;
  win.buf_count = 0;
  win.ex = false;

  signal(SIGWINCH, sig_resize);
  pos_T dir = {.lnum = 0, .col = 0};
  window_add_buffer(dir);

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
    ex_input(win.focus, key);
  }
  else {
    if (key == ';') {
      window_ex_cmd_start();
    }
    if (key == 'L') {
      if (win.focus->child) {
        win.focus = win.focus->child;
      }
    }
    if (key == 'H') {
      if (win.focus->parent) {
        win.focus = win.focus->parent;
      }
    }
    buf_input(win.focus, key);
  }
}

static void win_new(Token *arg, int cmd_flags)
{
  pos_T dir;
  ((int*)(&dir))[cmd_flags] = 1;
  if (win.buf_count > 1)
    window_add_buffer(dir);
  // TODO: replace with cntlr name lookup.
  //       load. init here. add to cntlr list etc.
  //       open. create instance.
  //       close. close instance.
  //       unload. cleanup here. remove from cntlr list.
  if (strcmp(arg->var.vval.v_string, "fm") == 0) {
    fm_init(win.focus->buf);
  }
  else if (strcmp(arg->var.vval.v_string, "sh") == 0) {
    fm_init(win.focus->buf);
  }
}

static void win_close(Token *arg, int cmd_flags)
{
}

void window_ex_cmd_start()
{
  win.ex = true;
  start_ex_cmd();
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
  }
}

void window_req_draw(Buffer *buf, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(&win.loop.events, cb, 1, buf);
}

void window_draw_all()
{
  log_msg("WINDOW", "DRAW_ALL");
  BufferNode *it = win.focus; 
  for (int i = 0; i < win.buf_count; ++i) {
    buf_refresh(it->buf);
    it = it->parent;
  }
}

void window_add_buffer(pos_T dir)
{
  log_msg("WINDOW", "PANE +ADD+");
  BufferNode *cn = malloc(sizeof(BufferNode*));
  cn->buf = buf_init();
  if (!win.focus) {
    cn->parent = NULL;
    cn->child = NULL;
    cn->buf = cn->buf;
  }
  else {
    //FIXME: swap creates abandoned nodes
    cn->parent = win.focus;
    cn->child = NULL;
    cn->buf = cn->buf;
    win.focus->child = cn;
  }
  win.focus = cn;
  win.buf_count++;
  layout_add_buffer(cn, win.buf_count, dir);
}
