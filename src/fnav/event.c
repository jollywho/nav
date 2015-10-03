#include <stdlib.h>
#include <assert.h>

#include <uv.h>
#include <ncurses.h>

#include "fnav/pane.h"
#include "fnav/event.h"
#include "fnav/log.h"

uv_loop_t *loop;
uv_timer_t draw_timer;

int event_init(void)
{
  log_msg("INIT", "event");
  uv_loop_init(loop);
  uv_timer_init(loop, &draw_timer);

  return 0;
}

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

uv_loop_t *eventloop(void)
{
  return loop;
}

void start_event_loop(void)
{
  uv_run(loop, UV_RUN_DEFAULT);
}
