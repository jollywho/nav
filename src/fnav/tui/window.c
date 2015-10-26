// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/ex_cmd.h"

#include "fnav/event/shell.h"

struct Window {
  Loop loop;
  BufferNode *blist;
  BufferNode *focus;
  uv_timer_t draw_timer;
  int buf_count;
  bool ex;
};

Window win;
Shell *sh; //TODO: testing only

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
  win.blist = NULL;
  win.buf_count = 0;
  win.ex = false;

  signal(SIGWINCH, sig_resize);
  window_add_buffer();
}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  if (key == '1') {
    sh = shell_new();
    shell_start(sh);
  }
  if (key == '2') {
    shell_stop(sh);
  }
  if (key == 'v') {
    window_add_buffer();
    ex_input(win.focus, 'o');
    return;
  }
  if (key == 'L') {
    win.focus = win.focus->next;
  }
  if (key == 'H') {
    win.focus = win.focus->prev;
  }

  if (win.ex) {
    ex_input(win.focus, key);
  }
  else {
    buf_input(win.focus, key);
  }
}

void window_ex_cmd_start()
{
  win.ex = true;
  // for now use mockup input
  ex_input(win.focus, 'o');
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

void window_add_buffer()
{
  log_msg("INIT", "PANE +ADD+");
  BufferNode *cn = malloc(sizeof(BufferNode*));
  cn->buf = buf_init();
  if (!win.blist) {
    cn->buf = cn->buf;
    cn->next = cn;
    cn->prev = cn;
    win.blist = cn;
  }
  else {
    cn->buf = cn->buf;
    cn->prev = win.blist->prev;
    cn->next = win.blist;
    win.blist->prev->next = cn;
    win.blist->prev = cn;
  }
  win.focus = cn;
  win.buf_count++;
  pos_T t = {.lnum = 0, .col = 1};
  layout_add_buffer(cn, win.buf_count, t);
}
