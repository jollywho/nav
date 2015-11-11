#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include <ncurses.h>
#include "fnav/table.h"

typedef struct BufferNode BufferNode;
typedef struct LineMatch LineMatch;

typedef void (*buffer_input_cb)(BufferNode *buf, int key);

struct BufferNode {
  BufferNode *parent;
  BufferNode *child;
  Buffer *buf;
};
struct Buffer {
  WINDOW *nc_win;
  BufferNode *bn;
  Cntlr *cntlr;

  buffer_input_cb input_cb;
  LineMatch *matches;

  pos_T b_size;
  pos_T b_ofs;
  pos_T cur;

  int lnum; // cursor
  int top;  // index

  fn_handle *hndl;
  bool dirty;
  bool queued;
  bool attached;
  bool closed;
};

Buffer* buf_init();
void buf_cleanup(Buffer *buf);

Cntlr *buf_cntlr(Buffer *buf);

void buf_set_cntlr(Buffer *buf, Cntlr *cntlr);
void buf_set_size(Buffer *buf, int r, int c);
void buf_set_ofs(Buffer *buf, int y, int x);
void buf_set_pass(Buffer *buf);

void buf_set_linematch(Buffer *buf, LineMatch *match);

void buf_full_invalidate(Buffer *buf, int index, int lnum);
int buf_input(BufferNode *bn, int key);

void buf_refresh(Buffer *buf);

void buf_move(Buffer *buf, int y, int x);

int buf_line(Buffer *buf);
int buf_top(Buffer *buf);
pos_T buf_size(Buffer *buf);
pos_T buf_ofs(Buffer *buf);

#endif
