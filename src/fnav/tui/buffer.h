#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include <ncurses.h>
#include "fnav/table.h"

typedef struct LineMatch LineMatch;

typedef void (*buffer_input_cb)(Buffer *buf, int key);

struct Buffer {
  WINDOW *nc_win;
  Cntlr *cntlr;
  Overlay *ov;

  buffer_input_cb input_cb;
  LineMatch *matches;

  pos_T b_size;
  pos_T b_ofs;

  int lnum; // cursor
  int top;  // index

  int ldif;

  fn_handle *hndl;
  bool dirty;
  bool queued;
  bool attached;
  bool closed;
  bool focused;

  int col_select;
  int col_text;
  int col_dir;
};

enum move_dir {
  MOVE_UP,
  MOVE_DOWN,
  MOVE_LEFT,
  MOVE_RIGHT,
};
enum dir_type { L_HORIZ, L_VERT };

void buf_init();
Buffer* buf_new();
void buf_delete(Buffer *buf);

Cntlr* buf_cntlr(Buffer *buf);

void buf_set_overlay(Buffer *buf, Overlay *ov);
void buf_set_cntlr(Buffer *buf, Cntlr *cntlr);
void buf_set_size_ofs(Buffer *buf, pos_T size, pos_T ofs);

void buf_set_pass(Buffer *buf);

void buf_set_linematch(Buffer *buf, LineMatch *match);
void buf_set_status(Buffer *buf, String name, String usr, String in, String out);

void buf_full_invalidate(Buffer *buf, int index, int lnum);
int buf_input(Buffer *bn, int key);

void buf_refresh(Buffer *buf);
void buf_toggle_focus(Buffer *buf, int focus);

void buf_move(Buffer *buf, int y, int x);
void buf_scroll(Buffer *buf, int y, int max);

int buf_line(Buffer *buf);
int buf_top(Buffer *buf);
pos_T buf_pos(Buffer *buf);
pos_T buf_size(Buffer *buf);
pos_T buf_ofs(Buffer *buf);

int buf_attached(Buffer *buf);

#endif
