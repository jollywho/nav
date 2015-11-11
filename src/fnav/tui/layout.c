// layout
// buffer tiling management
#include <ncurses.h>

#include "fnav/tui/layout.h"
#include "fnav/log.h"

pos_T layout_size()
{
  pos_T size;
  getmaxyx(stdscr, size.lnum, size.col);
  return size;
}

void layout_balance(BufferNode *focus, int count, pos_T dir)
{
  pos_T max = layout_size();
  max.lnum -= 1; // cmdline
  if (count == 1) {
    buf_set_size(focus->buf, max.lnum, max.col);
  }
  else {
    pos_T oldsize = buf_size(focus->parent->buf);
    int lines = (oldsize.lnum / (1 + dir.lnum));
    int cols =  (oldsize.col  / (1 + dir.col));
    buf_set_size(focus->buf, lines, cols);
    buf_set_size(focus->parent->buf, lines, cols);
    pos_T oldofs = buf_ofs(focus->parent->buf);
    int y =  (oldofs.lnum + lines);
    int x =  (oldofs.col  + cols);
    log_msg("BUFFER", "oldofs %d %d", oldofs.lnum, oldofs.col);
    log_msg("BUFFER", "newofs %d %d", x,y);
    buf_set_ofs(focus->buf, oldofs.lnum * dir.lnum, oldofs.col * dir.col);
    buf_set_ofs(focus->parent->buf, y * dir.lnum, x * dir.col);
  }
}
