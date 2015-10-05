#include <stdlib.h>
#include <assert.h>

#include <uv.h>
#include <ncurses.h>

#include "fnav/pane.h"
#include "fnav/event.h"
#include "fnav/log.h"

uv_loop_t loop;
uv_idle_t worker;
uv_timer_t gay;

void main_event_loop(uv_idle_t *handle)
{
  doloops();
}

void event_init(void)
{
  log_msg("INIT", "event");
  uv_loop_init(&loop);

  uv_idle_init(&loop, &worker);
  uv_idle_start(&worker, main_event_loop);
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
  log_msg("EVENT", "::::exited:::::");
}
