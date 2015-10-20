#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include "fnav/table.h"

typedef struct BufferNode BufferNode;

struct BufferNode {
  BufferNode *prev;
  BufferNode *next;
  fn_buf *buf;
  Cntlr *cntlr;
};

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;


fn_buf* buf_init();
void buf_destroy(fn_buf *buf);

void buf_set_cntlr(fn_buf *buf, Cntlr *cntlr);
void buf_set(fn_handle *hndl, String fname);
void buf_mv(fn_buf *buf, int x, int y);

String buf_val(fn_handle *hndl, String fname);
fn_rec* buf_rec(fn_handle *hndl);

int buf_pgsize(fn_handle *hndl);
int buf_entsize(fn_handle *hndl);
void buf_refresh(fn_buf *buf);

#endif
