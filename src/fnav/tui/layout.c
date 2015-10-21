// layout
// buffer tiling management
#include <ncurses.h>

#include "fnav/tui/layout.h"
#include "fnav/log.h"

void layout_add_buffer(BufferNode *focus, int count, pos_T dir)
{
  int maxh, maxw;
  getmaxyx(stdscr, maxh, maxw);
  int dw = (count * dir.col);
  int dh = (count * dir.lnum);
  int w = dw ? maxw / dw : 0;
  int h = dh ? maxh / dh : 0;
  if (count == 1) {
    buf_set_size(focus->buf, maxw, maxh);
  }
  else {
    // FIXME: table listener singleton is overwritten on second fm_cntlr.
    buf_set_size(focus->next->buf, w, 0);
    buf_set_ofs(focus->next->buf, w, h);
    buf_set_size(focus->buf, w, maxh);
  }
}
