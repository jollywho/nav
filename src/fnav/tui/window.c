// window module to manage draw loop.
// all ncurses drawing should be done here.
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"

#include "fnav/event/shell.h"

struct Window {
  Loop loop;
  BufferNode *blist;
  BufferNode *focus;
  uv_timer_t draw_timer;
};

Window win;
Shell *sh; //FIXME: testing purposes only

static void window_loop(Loop *loop, int ms);

void sig_resize(int sig)
{
  log_msg("WINDOW", "Signal received: **term resize**");
  // TODO: redo layout
}

void window_init(void)
{
  log_msg("INIT", "window");
  loop_add(&win.loop, window_loop);
  uv_timer_init(&win.loop.uv, &win.loop.delay);
  uv_timer_init(eventloop(), &win.draw_timer);
  win.focus = NULL;
  win.blist = NULL;

  signal(SIGWINCH, sig_resize);
}

#include "fnav/tui/fm_cntlr.h"

void window_input(int key)
{
  log_msg("WINDOW", "input");
  if (key == '1') {
    sh = shell_new();
    shell_start(sh);
  }
  if (key == '2') {
    shell_stop(sh);
  }
  if (key == 'o') {
    fn_buf *buf = buf_init();
    window_add_buffer(buf);
    // TODO: excmd would normally init a cntlr here
    win.focus->cntlr = &fm_cntlr_init(buf)->base;
  }
  if (win.focus->cntlr) {
    win.focus->cntlr->_input(win.focus->cntlr, key);
  }
  else {
    // buffer input
  }
}

static void window_loop(Loop *loop, int ms)
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

void window_add_buffer(fn_buf *buf)
{
  log_msg("INIT", "PANE +ADD+");
  BufferNode *cn = malloc(sizeof(BufferNode*));
  if (!win.blist) {
    cn->buf = buf;
    cn->next = cn;
    cn->prev = cn;
    win.blist = cn;
    win.focus = cn;
  }
  else {
    cn->buf = buf;
    cn->prev = win.blist->prev;
    cn->next = win.blist;
    win.blist->prev->next = cn;
    win.blist->prev = cn;
  }
}
