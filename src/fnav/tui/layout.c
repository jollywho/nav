// layout
// buffer tiling management
#include <ncurses.h>

#include "fnav/tui/layout.h"
#include "fnav/log.h"

void layout_add_buffer(BufferNode *focus, int count, pos_T dir)
{
  int maxr, maxc;
  getmaxyx(stdscr, maxr, maxc);
  int dr = (count * dir.lnum);
  int dc = (count * dir.col);
  int r = dr ? maxr / dr : 0;
  int c = dc ? maxc / dc : 0;
  if (count == 1) {
    buf_set_size(focus->buf, maxr, maxc);
  }
  else {
    buf_set_size(focus->next->buf, maxr, c);
    buf_set_ofs(focus->next->buf, r, c);
    buf_set_size(focus->buf, maxr, c);
  }
}
