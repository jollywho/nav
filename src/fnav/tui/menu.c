#include "fnav/tui/menu.h"
#include "fnav/compl.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/layout.h"

WINDOW *nc_win;
pos_T size;

void menu_start()
{
  // TODO: calc ideal menu size
  // resize layout to fit menu
  size = layout_size();
  nc_win = newwin(1, 0, size.lnum, size.col);
  // attempt to validate cmdstr in cmd
  // create context from valid results
}

void menu_stop()
{
}

static void menu_draw()
{
  // TODO:
  //
  // compl_list portion:
  //  keys | divider | column
  //  *selected row alternative colorscheme
  //
  // context portion:
  //  context | divider | tail arg
  //  or
  //  tail arg
}
