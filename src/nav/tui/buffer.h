#ifndef FN_TUI_BUFFER_H
#define FN_TUI_BUFFER_H

#include <ncurses.h>
#include "nav/regex.h"
#include "nav/filter.h"

struct Buffer {
  int id;
  WINDOW *nc_win;
  Plugin *plugin;
  Overlay *ov;

  LineMatch *matches;
  Filter *filter;

  pos_T b_size;
  pos_T b_ofs;

  int lnum; // cursor
  int top;  // index

  int ldif;

  fn_handle *hndl;
  bool dirty;
  bool queued;
  bool del;
  bool attached;
  bool nodraw;
  bool flat;

  short col_focus;
  short col_text;
  short col_dir;
  short col_sz;
};

enum move_dir { MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT };
enum dir_type { L_HORIZ, L_VERT };

void buf_init();
void buf_cleanup();
Buffer* buf_new();
void buf_delete(Buffer *buf);
void buf_detach(Buffer *buf);

Plugin* buf_plugin(Buffer *buf);
WINDOW* buf_ncwin(Buffer *buf);

void buf_set_plugin(Buffer *buf, Plugin *plugin);
void buf_set_size_ofs(Buffer *buf, pos_T size, pos_T ofs);

void buf_set_pass(Buffer *buf);
void buf_set_flat(Buffer *buf);
void buf_set_linematch(Buffer *buf, LineMatch *match);
void buf_set_status(Buffer *buf, char *, char *, char *);

void buf_update_progress(Buffer *buf, long);
void buf_full_invalidate(Buffer *buf, int index, int lnum);
int buf_input(Buffer *bn, Keyarg *ca);

void buf_refresh(Buffer *buf);
void buf_toggle_focus(Buffer *buf, int focus);
void buf_signal_filter(Buffer *buf, int count);

void buf_move_invalid(Buffer *buf, int index, int lnum);
void buf_move(Buffer *buf, int y, int x);
void buf_scroll(Buffer *buf, int y, int max);

void buf_end_sel(Buffer *buf);
void buf_g(void *, Keyarg *);
void buf_mark(void *, Keyarg *);
void buf_gomark(void *, Keyarg *);
void buf_yank(void *, Keyarg *);
void buf_del(void *, Keyarg *);

int buf_index(Buffer *buf);
int buf_line(Buffer *buf);
int buf_top(Buffer *buf);
int buf_id(Buffer *buf);
int buf_sel_count(Buffer *buf);
pos_T buf_pos(Buffer *buf);
pos_T buf_size(Buffer *buf);
pos_T buf_ofs(Buffer *buf);

int buf_attached(Buffer *buf);

typedef char* (*select_cb)(void *);
varg_T buf_select(Buffer *buf, const char *fld, select_cb cb);
void buf_sort(Buffer *buf, char *fld, int flags);

#endif
