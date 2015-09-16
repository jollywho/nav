#include <curses.h>

#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/buffer.h"
#include "fnav/loop.h"

struct fn_buf {
  WINDOW *nc_win;
  int b_offset;
  pos_T b_curs_pos;
  pos_T b_size;
  tentry *b_entf;
  int inval;
};

fn_buf* buf_init()
{
  printf("buf init\n");
  fn_buf *buf = malloc(sizeof(fn_buf));
  buf->inval = false;
  return buf;
}

void buf_inv(fn_handle *h)
{
  log_msg("BUFFER", "invalidated");
  if (h->buf->inval == false) {
    h->buf->inval = true;
    queue_push_buf(h->buf);
  }
}

void buf_draw(fn_buf *buf)
{
  log_msg("BUFFER", "$_draw_$");
  buf->inval = false;
}
