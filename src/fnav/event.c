#include <stdlib.h>
#include <assert.h>

#include <uv.h>
#include <ncurses.h>

#include "fnav/pane.h"
#include "fnav/event.h"
#include "fnav/log.h"

uv_loop_t loop;

void event_init(void)
{
  log_msg("INIT", "event");
  uv_loop_init(&loop);
}

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

uv_loop_t *eventloop(void)
{
  return &loop;
}

void start_event_loop(void)
{
  log_msg("EVENT", "::::started::::");
  uv_run(&loop, UV_RUN_DEFAULT);
}
