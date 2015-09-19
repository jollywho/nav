#ifndef FN_GUI_BUFFER_H
#define FN_GUI_BUFFER_H

#include "fnav/table.h"

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;

// buf_use(lst)       // cntrl uses this
// buf_filter(str)    // arrow uses this
// buf_mv_curs(pos_T)

fn_buf* buf_init(fn_tbl *t);
void buf_inv(Job *job, JobArg *arg);
void buf_draw(Job *job, JobArg *arg);
void buf_set(fn_buf *buf, fn_handle *th);
void buf_mv(fn_buf *buf, int x, int y);

#endif
