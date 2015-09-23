#include <stdlib.h>
#include <assert.h>

#include <uv.h>
#include <termkey.h>
#include <ncurses.h>

#include "fnav/event.h"
#include "fnav/log.h"

Loop *event_loop;
uv_timer_t fast_timer;

uv_poll_t poll_handle;

TermKey *tk;
char buffer[50];
void event_input(uv_poll_t *req, int status, int events);

int event_init(void)
{
  log_msg("INIT", "event");
  event_loop = uv_default_loop();
  uv_timer_init(event_loop, &fast_timer);

  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8);

  uv_poll_init(event_loop, &poll_handle, 0);
  uv_poll_start(&poll_handle, UV_READABLE, event_input);
  return 0;
}

void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
  buf->base = malloc(size);
  buf->len = size;
}

// TODO: reset after async if key held prior
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
      rpc_key_handle(key.utf8);
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

void loop_timeout(uv_timer_t *req)
{
  log_msg("EVENT", "++cycle timeout++");
  stop_cycle();
}

void start_event_loop(void)
{
  log_msg("EVENT", "<<|MAIN EVENT LOOP|>>");
  uv_run(event_loop, UV_RUN_DEFAULT);
}

void stop_cycle(void)
{
  log_msg("EVENT", "<<disable>>");
  uv_timer_stop(&fast_timer);
}

void cycle_events(void)
{
  log_msg("EVENT", "<<enable>>");
  uv_timer_start(&fast_timer, loop_timeout, 10, 0);
  uv_run(event_loop, UV_RUN_ONCE);
}

Loop *eventloop(void)
{
  return event_loop;
}
