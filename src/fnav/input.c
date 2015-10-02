
#include <uv.h>
#include <termkey.h>

#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/pane.h"

#define INPUT_INTERVAL (uint32_t)30

uv_timer_t input_timer;
uv_loop_t *loop;
uv_poll_t poll_handle;

TermKey *tk;
char buffer[50];
void event_input(uv_poll_t *req, int status, int events);

void input_check()
{
  global_input_time = os_hrtime();
  uv_run(loop, UV_RUN_ONCE);
}

void input_init(void)
{
  log_msg("INIT", "input");
  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8);

  uv_poll_init(loop, &poll_handle, 0);
  uv_poll_start(&poll_handle, UV_READABLE, event_input);

  uv_timer_init(eventloop(), &input_timer);
  uv_timer_start(&input_timer, input_check, INPUT_INTERVAL, INPUT_INTERVAL);
}

void event_input(uv_poll_t *req, int status, int events)
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
      //keep this for testing
      fprintf(stderr, "Unrecognised CSI %c %ld;%ld %c%c\n",
          (char)(command >> 8), args[0], args[1], (char)(command >> 16), (char)command);
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
      // printf("\033[?6n"); // DECDSR 6 == request cursor position
      printf("\033[?1$p"); // DECRQM == request mode, DEC origin mode
      fflush(stdout);
    }
  }
}
