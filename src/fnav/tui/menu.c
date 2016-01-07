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
  bool docmpl;
  bool rebuild;

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
  mnu->rebuild = false;

  menu_restart(mnu);

  return mnu;
}

void menu_restart(Menu *mnu)
{
  if (!mnu) return;
  mnu->cx = context_start();
  mnu->docmpl = false;
  ex_cmd_push(mnu->cx);
  compl_build(mnu->cx, "");
  compl_update(mnu->cx, "");
}

static void rebuild_contexts(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "rebuild_contexts");
  Token *word;
  int i = 0;
  int pos = 0;
  while ((word = cmdline_tokindex(cmd, i))) {
    String key = TOKEN_STR(word->var);
    fn_context *find = find_context(mnu->cx, key);
    if (find) {
      mnu->cx = find->params[0];
    }
    pos = word->end;
    ex_cmd_set(pos);
    ex_cmd_push(mnu->cx);
    i++;
  }
  if (i > 0) {
    mnu->cx = ex_cmd_pop(1)->cx;
    ex_cmd_set(pos - 1);
    compl_build(mnu->cx, ex_cmd_curstr());
    compl_update(mnu->cx, ex_cmd_curstr());
  }
  mnu->rebuild = false;
}

void menu_rebuild(Menu *mnu)
{
  menu_restart(mnu);
  mnu->rebuild = true;
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

  if (mnu->rebuild)
    return rebuild_contexts(mnu, cmd);
  if (ex_cmd_state() & EX_CYCLE) {
    return;
  }
  mnu->lnum = 0;

  if ((ex_cmd_state() & EX_POP) == EX_POP) {
    mnu->cx = ex_cmd_pop(1)->cx;
    mnu->docmpl = true;
  }
  else if ((ex_cmd_state() & EX_PUSH) == EX_PUSH) {
    String key = ex_cmd_curstr();
    fn_context *find = find_context(mnu->cx, key);
    if (find) {
      mnu->cx = find->params[0];
      mnu->docmpl = true;
    }
    else {
      mnu->cx = NULL;
    }
    ex_cmd_push(mnu->cx);
  }
  if (mnu->docmpl)
    compl_build(mnu->cx, ex_cmd_curstr());
  compl_update(mnu->cx, ex_cmd_curstr());
  mnu->docmpl = false;
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

static String cycle_matches(Menu *mnu, int dir)
{
  fn_compl *cmpl = mnu->cx->cmpl;

  String before = cmpl->matches[mnu->lnum]->key;
  mnu->lnum += dir;

  if (mnu->lnum < 0)
    mnu->lnum = cmpl->matchcount - 1;
  if (mnu->lnum > cmpl->matchcount - 1)
    mnu->lnum = 0;
  return before;
}

String menu_next(Menu *mnu, int dir)
{
  log_msg("MENU", "menu_curkey");
  if (!mnu->cx || !mnu->cx->cmpl) {
    return NULL;
  }
  fn_compl *cmpl = mnu->cx->cmpl;
  log_msg("MENU", "%d %d", mnu->lnum, cmpl->matchcount);
  if (cmpl->matchcount < 1) return NULL;

  String before = cycle_matches(mnu, dir);

  if (strcmp(ex_cmd_curstr(), before) == 0)
    before = cycle_matches(mnu, dir);

  return before;
}
