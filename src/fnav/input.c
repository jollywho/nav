
#include <uv.h>
#include <termkey.h>

#include "fnav/input.h"
#include "fnav/window.h"
#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/pane.h"

uv_timer_t input_timer;
uv_poll_t poll_handle;

TermKey *tk;
char buffer[50];
void event_input();

void input_check()
{
  global_input_time = os_hrtime();
  event_input();
}

void input_init(void)
{
  log_msg("INIT", "INPUT");
  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8);

  uv_timer_init(eventloop(), &input_timer);
  uv_timer_start(&input_timer, input_check, INPUT_INTERVAL, INPUT_INTERVAL);
}

void event_input()
{
  TermKeyKey key;
  TermKeyFormat format = TERMKEY_FORMAT_URWID;
  TermKeyResult ret;

  termkey_advisereadable(tk);
  ret = termkey_getkey_force(tk, &key);
  if(ret == TERMKEY_RES_KEY) {
    termkey_strfkey(tk, buffer, sizeof buffer, &key, format);

    if(key.type == TERMKEY_TYPE_UNKNOWN_CSI) {
      long args[16];
      size_t nargs = 16;
      unsigned long command;
      termkey_interpret_csi(tk, &key, args, &nargs, &command);
    }
    else {
      if (key.modifiers) {
        unsigned int mask = (1 << key.modifiers) -1;
        key.code.number &= key.code.number & mask;
      }
      window_input(key.code.number);
    }

    if(key.type == TERMKEY_TYPE_UNICODE &&
        key.modifiers & TERMKEY_KEYMOD_CTRL &&
        (key.code.codepoint == 'C' || key.code.codepoint == 'c'))
      exit(0);

    if(key.type == TERMKEY_TYPE_UNICODE &&
        key.modifiers == 0 &&
        key.code.codepoint == '?') {
    }
  }
}
