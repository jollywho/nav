#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include <ncurses.h>
#include "fnav/table.h"

typedef struct LineMatch LineMatch;

typedef void (*buffer_input_cb)(Buffer *buf, int key);

struct Buffer {
  WINDOW *nc_win;
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

enum move_dir {
  MOVE_UP,
  MOVE_DOWN,
  MOVE_LEFT,
  MOVE_RIGHT,
};
enum dir_type { L_HORIZ, L_VERT };

Buffer* buf_init();
void buf_cleanup(Buffer *buf);

Cntlr *buf_cntlr(Buffer *buf);

void buf_set_cntlr(Buffer *buf, Cntlr *cntlr);
void buf_set_size(Buffer *buf, pos_T size);
void buf_set_ofs(Buffer *buf, pos_T pos);

void buf_set_pass(Buffer *buf);

void buf_set_linematch(Buffer *buf, LineMatch *match);

void buf_full_invalidate(Buffer *buf, int index, int lnum);
int buf_input(Buffer *bn, int key);

void buf_refresh(Buffer *buf);

void buf_move(Buffer *buf, int y, int x);

int buf_line(Buffer *buf);
int buf_top(Buffer *buf);
pos_T buf_pos(Buffer *buf);
pos_T buf_size(Buffer *buf);
pos_T buf_ofs(Buffer *buf);

int buf_attached(Buffer *buf);

#endif
