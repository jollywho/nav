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
  bool inval;
};

fn_buf* buf_init()
{
  printf("buf init\n");
  fn_buf *buf = malloc(sizeof(fn_buf));
  buf->inval = false;
  return buf;
}

void buf_inv(Job *job, JobArg *arg)
{
  log_msg("BUFFER", "invalidated");
  if (job->hndl->buf->inval == false) {
    job->hndl->buf->inval = true;
    arg->fn = buf_draw;
    QUEUE_PUT(draw, job, arg);
  }
}

void buf_draw(Job *job, JobArg *arg)
{
  log_msg("BUFFER", "$_draw_$");
  job->hndl->buf->inval = false;
}
