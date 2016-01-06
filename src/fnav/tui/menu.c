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

  pos_T size;
  pos_T ofs;

  int row_max;
  int lnum;

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

  mnu->lnum = 0;

  menu_restart(mnu);

  return mnu;
}

void menu_restart(Menu *mnu)
{
  mnu->cx = context_start();
  ex_cmd_push(mnu->cx);
  compl_build(mnu->cx, "");
  compl_update(mnu->cx, "");
}

void menu_stop(Menu *mnu)
{
  log_msg("MENU", "menu_stop");
  werase(mnu->nc_win);
  wnoutrefresh(mnu->nc_win);

  delwin(mnu->nc_win);
  free(mnu);
  window_shift(ROW_MAX+1);
}

void menu_update(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "menu_update");
  log_msg("MENU", "##%d", ex_cmd_state());

  if ((ex_cmd_state() & EX_CYCLE) == EX_CYCLE) {
    return;
  }
  mnu->lnum = 0;

  if ((ex_cmd_state() & EX_POP) == EX_POP) {
    mnu->cx = ex_cmd_pop(1)->cx;
    compl_build(mnu->cx, ex_cmd_curstr());
  }
  else if ((ex_cmd_state() & EX_PUSH) == EX_PUSH) {
    String key = ex_cmd_curstr();
    fn_context *find = find_context(mnu->cx, key);
    if (find)
      mnu->cx = find->params[0];
    else {
      mnu->cx = NULL;
    }
    ex_cmd_push(mnu->cx);
    compl_build(mnu->cx, ex_cmd_curstr());
  }
  compl_update(mnu->cx, ex_cmd_curstr());
}

void menu_draw(Menu *mnu)
{
  log_msg("MENU", "menu_draw");

  werase(mnu->nc_win);

  wattron(mnu->nc_win, COLOR_PAIR(mnu->col_line));
  mvwhline(mnu->nc_win, ROW_MAX, 0, ' ', mnu->size.col);
  wattroff(mnu->nc_win, COLOR_PAIR(mnu->col_line));

  if (!mnu->cx || !mnu->cx->cmpl) {
    wnoutrefresh(mnu->nc_win);
    return;
  }
  fn_compl *cmpl = mnu->cx->cmpl;

  for (int i = 0; i < ROW_MAX && i < cmpl->matchcount; i++) {

    compl_item *row = cmpl->matches[i];

    DRAW_STR(mnu, nc_win, i, 0, ">", col_div);
    DRAW_STR(mnu, nc_win, i, 2, row->key, col_text);

    for (int c = 0; c < row->colcount; c++) {
      String col = row->columns[i];
      DRAW_STR(mnu, nc_win, i, 2, col, col_text);
    }
  }
  String key = mnu->cx->key;
  DRAW_STR(mnu, nc_win, ROW_MAX, 1, key, col_line);

  wnoutrefresh(mnu->nc_win);
}

String menu_next(Menu *mnu)
{
  log_msg("MENU", "menu_curkey");
  if (!mnu->cx || !mnu->cx->cmpl) {
    return NULL;
  }
  fn_compl *cmpl = mnu->cx->cmpl;
  log_msg("MENU", "%d %d", mnu->lnum, cmpl->matchcount);
  if (cmpl->matchcount < 1) return NULL;

  String before = cmpl->matches[mnu->lnum]->key;

  mnu->lnum++;

  if (mnu->lnum > cmpl->matchcount - 1)
    mnu->lnum = 0;
  return before;
}
