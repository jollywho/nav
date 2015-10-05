// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/window.h"
#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/pane.h"

struct Window {
  Loop loop;
  Pane pane;
};

Window win;
void window_loop(Loop *loop);

void window_init(void)
{
  log_msg("INIT", "window");
  loop_init(&win.loop, window_loop);
  pane_init(&win.pane);
}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  pane_input(&win.pane, key);
}

void window_loop(Loop *loop)
{
  if (!queue_empty(win.loop.events)) {
    process_loop(&win.loop);
    doupdate();
  }
}

void window_req_draw(fn_buf *buf, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(win.loop.events, cb, 1, buf);
}
