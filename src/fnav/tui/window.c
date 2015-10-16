// window module to manage draw loop.
// all ncurses drawing should be done here.
#define _GNU_SOURCE
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/tui/pane.h"

struct Window {
  Loop loop;
  Pane pane;
  uv_timer_t draw_timer;
};


Loop temp;

Window win;
void window_loop(Loop *loop, int ms);

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  // TODO: redo layout
}

void temploop(Loop *loop, int ms)
{
  process_loop(&temp, ms);
}

void window_init(void)
{
  log_msg("INIT", "window");
  loop_add(&win.loop, window_loop);
  uv_timer_init(&win.loop.uv, &win.loop.delay);
  uv_timer_init(eventloop(), &win.draw_timer);
  pane_init(&win.pane);

  loop_add(&temp, temploop);
  signal(SIGWINCH, sig_resize);
}

#include <malloc.h>
#include "fnav/event/uv_process.h"
#include "fnav/event/pty_process.h"
#include "fnav/event/rstream.h"

typedef struct {
  char *data;
  size_t cap, len;
} DynamicBuffer;

#define DYNAMIC_BUFFER_INIT {NULL, 0, 0}
char* p_sh = "/bin/sh";
Stream in, out, err;
DynamicBuffer buf = DYNAMIC_BUFFER_INIT;
stream_read_cb data_cb;
PtyProcess uvproc;
Process *proc;

static void out_data_cb(Stream *stream, RBuffer *buf, size_t count, void *data,
    bool eof)
{
  size_t cnt;
  char *ptr = rbuffer_read_ptr(buf, &cnt);

  if (!cnt) {
    return;
  }

  log_msg("out", "%s", ptr);
  //// No output written, force emptying the Rbuffer if it is full.
  //if (!written && rbuffer_size(buf) == rbuffer_capacity(buf)) {
  //  written = cnt;
  //}
  //if (written) {
  //  rbuffer_consumed(buf, written);
  //}
}

void doproc()
{
  char **rv = malloc((unsigned)((3) * sizeof(char *)));
  rv[0] = (char*)p_sh;
  rv[1] = "-c";
  rv[2] = "ls";
  rv[3] = NULL;
  data_cb = out_data_cb;
  uvproc = pty_process_init(&temp, &buf);
  proc = &uvproc.process;
  proc->out = &out;
  proc->in = &in;
  proc->err = &out;
  proc->argv = rv;
  if (!process_spawn(proc)) {
    log_msg("____", "cannot execute");
  }
  log_msg("WINDOW", "--%p--", &temp.events);
  log_msg("WINDOW", "--procend--");
  rstream_init(proc->out, &temp.events, 0);
  rstream_start(proc->out, data_cb);
  log_msg("WINDOW", "--end--");
  //FIX: failure point around rstream read/invoke push cb to queue

}

void window_input(int key)
{
  log_msg("WINDOW", "input");
  if (key == '1') {
    doproc();
  }
  pane_input(&win.pane, key);
}

void window_loop(Loop *loop, int ms)
{
  if (!queue_empty(&win.loop.events)) {
    process_loop(&win.loop, ms);
    doupdate();
  }
}

void window_req_draw(fn_buf *buf, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(&win.loop.events, cb, 1, buf);
}
