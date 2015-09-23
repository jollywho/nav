#ifndef FN_GUI_BUFFER_H
#define FN_GUI_BUFFER_H

#include "fnav/table.h"

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;


fn_buf* buf_init(fn_handle *hndl);
void buf_inv(Job *job, JobArg *arg);
void buf_draw(Job *job, JobArg *arg);
void buf_set(fn_handle *hndl, String fname);
void buf_mv(fn_buf *buf, int x, int y);
String buf_val(fn_handle *hndl, String fname);

#endif
