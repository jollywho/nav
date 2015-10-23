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
  int buf_count;
};

Window win;
Shell *sh; //TODO: testing only

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
  win.buf_count = 0;

  signal(SIGWINCH, sig_resize);
}

#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/layout.h"

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
    BufferNode *bn = window_add_buffer();
    // TODO: excmd would normally init a cntlr here
    win.focus->cntlr = (Cntlr*)fm_cntlr_init(bn->buf);
    return;
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

void window_req_draw(Buffer *buf, argv_callback cb)
{
  log_msg("WINDOW", "req draw");
  CREATE_EVENT(&win.loop.events, cb, 1, buf);
}

BufferNode* window_add_buffer()
{
  log_msg("INIT", "PANE +ADD+");
  BufferNode *cn = malloc(sizeof(BufferNode*));
  cn->buf = buf_init();
  if (!win.blist) {
    cn->buf = cn->buf;
    cn->next = cn;
    cn->prev = cn;
    win.blist = cn;
    win.focus = cn;
  }
  else {
    cn->buf = cn->buf;
    cn->prev = win.blist->prev;
    cn->next = win.blist;
    win.blist->prev->next = cn;
    win.blist->prev = cn;
  }
  win.buf_count++;
  pos_T t = {.lnum = 0, .col = 1};
  layout_add_buffer(cn, win.buf_count, t);
  return cn;
}
