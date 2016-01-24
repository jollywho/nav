#include <uv.h>
#include <ncurses.h>

#include "fnav/event/event.h"
#include "fnav/event/input.h"
#include "fnav/log.h"

static Loop loop;

void event_init(void)
{
  log_msg("INIT", "event");
  loop_add(&loop);
}

void event_cleanup(void)
{
  uv_loop_close(&loop.uv);
}

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

uv_loop_t *eventloop(void)
{
  return &loop.uv;
}

Loop* mainloop(void)
{
  return &loop;
}

Queue* drawq(void)
{
  return &loop.drawq;
}

Queue* eventq(void)
{
  return &loop.eventq;
}

void event_input(void)
{
  input_check();
}

void stop_event_loop(void)
{
  uv_stop(&loop.uv);
}

void start_event_loop(void)
{
  log_msg("EVENT", "::::started::::");
  uv_run(&loop.uv, UV_RUN_DEFAULT);
  log_msg("EVENT", "::::exited:::::");
}
