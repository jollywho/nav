#include "fnav/tui/menu.h"
#include "fnav/log.h"
#include "fnav/macros.h"
#include "fnav/compl.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
#include "fnav/option.h"

static const int ROW_MAX = 5;

struct Menu {
  WINDOW *nc_win;

  fn_context *cx;
  fn_compl *cmpl;

  pos_T size;
  pos_T ofs;
  int row_max;

  int col_text;
  int col_div;
  int col_select;
  int col_box;
  int col_line;
};

Menu* menu_start()
{
  log_msg("MENU", "menu_start");
  Menu *mnu = malloc(sizeof(Menu));

  pos_T size = layout_size();
  mnu->size = (pos_T){ROW_MAX+1, size.col};
  mnu->ofs =  (pos_T){size.lnum - (ROW_MAX+2), 0};
  window_shift(-(ROW_MAX+1));

  mnu->nc_win = newwin(
      mnu->size.lnum,
      mnu->size.col,
      mnu->ofs.lnum,
      mnu->ofs.col);

  mnu->col_select = attr_color("BufSelected");
  mnu->col_text   = attr_color("ComplText");
  mnu->col_div    = attr_color("OverlaySep");
  mnu->col_box    = attr_color("OverlayActive");
  mnu->col_line   = attr_color("OverlayLine");

  mnu->cx = context_start();
  mnu->cmpl = compl_create(mnu->cx, "");

  return mnu;
}

void menu_stop(Menu *mnu)
{
  log_msg("MENU", "menu_stop");
  werase(mnu->nc_win);
  wnoutrefresh(mnu->nc_win);
  // delete context
  // delete compl

  delwin(mnu->nc_win);
  free(mnu);
  window_shift(ROW_MAX+1);
}

void menu_update(Menu *mnu, Cmdline *cmd)
{
  // compare cmdline to context's cur_param
  // if first token in cmdline matches type of cur_param
  //  if cur_param has more params
  //    context = cur_param
  //  else if cur_param has next
  //    cur_param = next
  //
  // cmpl = compl_create(mnu->cx, cmdline)
}

void menu_draw(Menu *mnu)
{
  log_msg("MENU", "menu_draw");
  fn_compl *cmpl = mnu->cmpl;

  wattron(mnu->nc_win, COLOR_PAIR(mnu->col_line));
  mvwhline(mnu->nc_win, ROW_MAX, 0, ' ', mnu->size.col);
  wattroff(mnu->nc_win, COLOR_PAIR(mnu->col_line));

  if (cmpl) {
    for (int i = 0; i < cmpl->rowcount && i < ROW_MAX; i++) {
      compl_item *row = cmpl->rows[i];
      if (!row) break;
      DRAW_STR(mnu, nc_win, i, 0, ">", col_div);
      DRAW_STR(mnu, nc_win, i, 2, row->key, col_text);

      for (int i = 0; i < row->colcount; i++) {
        String col = row->columns[i];
        DRAW_STR(mnu, nc_win, i, 2, col, col_text);
      }
    }
    DRAW_STR(mnu, nc_win, ROW_MAX, 0, cmpl->name, col_box);
  }

  wnoutrefresh(mnu->nc_win);
}
