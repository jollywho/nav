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
  int r = dr ? maxr / dr : maxr;
  int c = dc ? maxc / dc : maxc;
  if (count == 1) {
    buf_set_size(focus->buf, maxr, maxc);
  }
  else {
    BufferNode *it = focus->next;
    buf_set_size(focus->buf, r, c);
    for (int i = 1; it != focus; it = it->next, ++i) {
      buf_set_size(it->buf, r, c);
      buf_set_ofs(it->buf, r * i * dir.lnum, c * i * dir.col);
    }
  }
}
