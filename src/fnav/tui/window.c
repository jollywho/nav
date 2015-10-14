// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/pane.h"

struct Window {
  Loop loop;
  Pane pane;
  uv_timer_t draw_timer;
};

Window win;
void window_loop(Loop *loop, int ms);

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
  pane_init(&win.pane);
  signal(SIGWINCH, sig_resize);
}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  pane_input(&win.pane, key);
}

void window_loop(Loop *loop, int ms)
{
  if (!queue_empty(&win.loop.events)) {
    process_loop(&win.loop, ms);
    doupdate();
  }
}

void window_req_draw(fn_buf *buf, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(&win.loop.events, cb, 1, buf);
}
