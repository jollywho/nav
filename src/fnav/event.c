#include <stdlib.h>
#include <assert.h>

#include <uv.h>
#include <ncurses.h>

#include "fnav/pane.h"
#include "fnav/event.h"
#include "fnav/log.h"

uv_loop_t loop;
uv_timer_t event_timer;
uint64_t before;
uint64_t after;
#define TIMESTEP (uint64_t)10

void main_event_loop(uv_timer_t *handle)
{
  log_msg("EVENT", "_:_:_main event loop_:_:_");
  before = os_hrtime();
  doloops(30);
  after = os_hrtime();
  int took = (int) ((after - before) / 1000000);
  int ms = TIMESTEP - took;
  if (ms < 0) ms = 0;
  uv_timer_start(&event_timer, main_event_loop, (uint64_t)ms, (uint64_t)ms);
}

void event_init(void)
{
  log_msg("INIT", "event");
  uv_loop_init(&loop);

  uv_timer_init(&loop, &event_timer);
  uv_timer_start(&event_timer, main_event_loop, TIMESTEP, TIMESTEP);
}

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

uv_loop_t *eventloop(void)
{
  return &loop;
}

void stop_event_loop(void)
{
  before = os_hrtime();
}

void start_event_loop(void)
{
  log_msg("EVENT", "::::started::::");
  uv_run(&loop, UV_RUN_DEFAULT);
  log_msg("EVENT", "::::exited:::::");
}
