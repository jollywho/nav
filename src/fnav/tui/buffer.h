#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include "fnav/table.h"

typedef struct BufferNode BufferNode;

struct BufferNode {
  BufferNode *prev;
  BufferNode *next;
  Buffer *buf;
  Cntlr *cntlr;
};

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;


Buffer* buf_init();
void buf_destroy(Buffer *buf);

void buf_set_cntlr(Buffer *buf, Cntlr *cntlr);
void buf_set(fn_handle *hndl, String fname);
void buf_set_size(Buffer *buf, int w, int h);
void buf_set_ofs(Buffer *buf, int x, int y);

void buf_full_invalidate(Buffer *buf, int index);
int buf_lines(Buffer *buf);

void buf_refresh(Buffer *buf);

#endif
