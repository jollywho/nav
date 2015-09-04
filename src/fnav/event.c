#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <uv.h>
#include <termkey.h>
#include <inttypes.h>
#include <curses.h>

#include "rpc.h"

#define ERROR(msg, code) do {                                                         \
  fprintf(stderr, "%s: [%s: %s]\n", msg, uv_err_name((code)), uv_strerror((code)));   \
  assert(0);                                                                          \
} while(0);

uv_loop_t *loop;
uv_timer_t tick;

TermKey *tk;
char buffer[50];

void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
  buf->base = malloc(size);
  buf->len = size;
}

void update(uv_timer_t *req)
{
  TermKeyKey key;
  TermKeyFormat format = TERMKEY_FORMAT_URWID;
  TermKeyResult ret;
  termkey_advisereadable(tk);
  ret = termkey_getkey(tk, &key);
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

int event_init(void)
{
  loop = uv_default_loop();
  uv_timer_init(loop, &tick);
  uv_timer_start(&tick, update, 10, 10);

  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8);

  return 0;
}

void start_event_loop(void)
{
  uv_run(loop, UV_RUN_DEFAULT);
}

void event_push(Channel channel)
{
  fprintf(stderr, "channel push\n");
  channel.open_cb(loop, channel);
}
