// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>
#include "fnav/event.h"
#include "fnav/pane.h"

#define DRAW_DELAY (uint32_t)30

Loop *loop;
Pane *pane;
uv_timer_t draw_timer;

void window_init(void)
{
  loop_init(loop);
  uv_timer_init(&loop->uv, &loop->delay);
  uv_timer_init(eventloop(), &draw_timer);
}

void window_input(int key)
{
}

void window_draw(uv_timer_t *req)
{
  // process event loop
  process_loop(loop);
  doupdate();
}

void window_req_draw(Window *w, fn_buf *buf, draw_cb cb)
{
  if (!queue_empty(loop->events)) {
    uv_timer_start(&draw_timer, window_draw, DRAW_DELAY, DRAW_DELAY);
  }
  CREATE_EVENT(loop->events, cb, buf);
}
