#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include "fnav/table.h"

typedef struct BufferNode BufferNode;

struct BufferNode {
  BufferNode *parent;
  BufferNode *child;
  Buffer *buf;
};

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;


Buffer* buf_init();
void buf_destroy(Buffer *buf);

void buf_set_cntlr(Buffer *buf, Cntlr *cntlr);
void buf_set(fn_handle *hndl);
void buf_set_size(Buffer *buf, int r, int c);
void buf_set_ofs(Buffer *buf, int y, int x);

void buf_full_invalidate(Buffer *buf, int index, int lnum);
int buf_input(BufferNode *bn, int key);

void buf_refresh(Buffer *buf);

int buf_line(Buffer *buf);
int buf_top(Buffer *buf);
pos_T buf_size(Buffer *buf);
pos_T buf_ofs(Buffer *buf);

#endif
