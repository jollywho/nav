// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/ex_cmd.h"

struct Window {
  Loop loop;
  BufferNode *focus;
  uv_timer_t draw_timer;
  int buf_count;
  bool ex;
};

Window win;

static void window_loop(Loop *loop, int ms);

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  // TODO: redo layout
}

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

void window_ex_cmd_start()
{
  win.ex = true;
  window_ex_cmd();
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
