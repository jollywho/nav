
#include <stdio.h>
#include <uv.h>
#include <termkey.h>

#include "fnav/event/input.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/log.h"
#include "fnav/tui/pane.h"

uv_poll_t poll_handle;
uv_loop_t loop;

TermKey *tk;
void event_input();

void input_check()
{
  log_msg("INPUT", "INPUT CHECK");
  event_input();
}

void input_init(void)
{
  log_msg("INIT", "INPUT");
  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8);

  uv_poll_init(eventloop(), &poll_handle, 0);
  uv_poll_start(&poll_handle, UV_READABLE, input_check);
}

void event_input()
{
  TermKeyKey key;
  TermKeyResult ret;

  termkey_advisereadable(tk);

  while ((ret = termkey_getkey_force(tk, &key)) == TERMKEY_RES_KEY) {
    if (key.modifiers) {
      unsigned int mask = (1 << key.modifiers) -1;
      key.code.number &= key.code.number & mask;
    }
    window_input(key.code.number);
  }
}
