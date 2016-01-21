#include "fnav/tui/menu.h"
#include "fnav/log.h"
#include "fnav/macros.h"
#include "fnav/compl.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/option.h"
#include "fnav/event/fs.h"

static const int ROW_MAX = 5;
static Menu *cur_menu;

struct Menu {
  WINDOW *nc_win;
  fn_handle *hndl;
  fn_fs *fs;

  fn_context *cx;
  bool docmpl;
  bool rebuild;
  String line_key;

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

static void menu_queue_draw(void **argv)
{
  menu_draw(argv[0]);
}

static void menu_fs_cb(void **args)
{
  log_msg("MENU", "menu_fs_cb");
  log_msg("MENU", "%s", cur_menu->hndl->key);
  compl_destroy(cur_menu->cx);
  ventry *ent = fnd_val("fm_files", "dir", cur_menu->hndl->key);
  int count = tbl_ent_count(ent);
  compl_new(count, COMPL_DYNAMIC);

  for (int i = 0; i < count; i++) {
    fn_rec *rec = ent->rec;
    compl_set_index(i, rec_fld(rec, "name"), 0, NULL);
    ent = ent->next;
  }
  // FIXME: should check if cx changed
  compl_force_cur(cur_menu->cx);
  compl_update(cur_menu->cx, cur_menu->line_key);
  window_req_draw(cur_menu, menu_queue_draw);
}

void menu_ch_dir(void **args)
{
  log_msg("MENU", "menu_ch_dir");
  String dir = args[1];
  if (!dir) return;
  fs_close(cur_menu->fs);
  fs_open(cur_menu->fs, dir);
  cur_menu->hndl->key = cur_menu->fs->path;
}

void path_list(List *args)
{
  log_msg("MENU", "path_list");
  if (!cur_menu) return;
  Cntlr *cntlr = window_get_cntlr();
  String dir, val = NULL;

  int exec = 0;
  int pos = ex_cmd_curidx();
  Token *tok = tok_arg(args, pos);

  if (!tok) {
    log_msg("MENU", "!TOK");
    dir = fm_cur_dir(cntlr);
    fs_read(cur_menu->fs, dir);
  }
  else {
    log_msg("MENU", "TOK!");

    dir = token_val(tok, VAR_STRING);
    cur_menu->line_key = dir;

    if (dir[0] == '@') {
      marklbl_list(args);
      compl_force_cur(cur_menu->cx);
      compl_update(cur_menu->cx, cur_menu->line_key);
      return;
    }

    if (dir[0] != '/') {
      dir = conspath(fm_cur_dir(cntlr), dir);
    }

    int prev = tok->block;
    while ((tok = tok_arg(args, ++pos))) {
      if (prev != tok->block)
        break;
      val = token_val(tok, VAR_STRING);

      if (val[0] == '/') {
        exec = 1;
        cur_menu->line_key = "";
        continue;
      }
      else {
        exec = 0;
        cur_menu->line_key = val;
        log_msg("MENU", "%s %s", dir, val);
        String tmp = conspath(dir, val);
        free(dir);
        dir = tmp;
      }
    }

    log_msg("MENU", "dir: %s", dir);
    log_msg("MENU", "key: %s", cur_menu->line_key);
    if (exec)
      fs_read(cur_menu->fs, dir);
  }
}

Menu* menu_new()
{
  log_msg("MENU", "menu_new");
  Menu *mnu = malloc(sizeof(Menu));
  fn_handle *hndl = malloc(sizeof(fn_handle));
  mnu->hndl = hndl;

  mnu->col_select = attr_color("BufSelected");
  mnu->col_text   = attr_color("ComplText");
  mnu->col_div    = attr_color("OverlaySep");
  mnu->col_box    = attr_color("OverlayActive");
  mnu->col_line   = attr_color("OverlayLine");

  memset(mnu->hndl, 0, sizeof(fn_handle));
  mnu->hndl->tn = "fm_files";
  mnu->hndl->key_fld = "dir";
  mnu->hndl->fname = "name";

  mnu->fs = fs_init(mnu->hndl);
  mnu->fs->open_cb = menu_fs_cb;
  mnu->fs->stat_cb = menu_ch_dir;
  mnu->fs->data = mnu;
  cur_menu = mnu;
  return mnu;
}

void menu_delete(Menu *mnu)
{
  free(mnu);
  cur_menu = NULL;
}

void menu_start(Menu *mnu)
{
  log_msg("MENU", "menu_start");

  pos_T size = layout_size();
  mnu->size = (pos_T){ROW_MAX+1, size.col};
  mnu->ofs =  (pos_T){size.lnum - (ROW_MAX+2), 0};
  window_shift(-(ROW_MAX+1));

  mnu->nc_win = newwin(
      mnu->size.lnum,
      mnu->size.col,
      mnu->ofs.lnum,
      mnu->ofs.col);

  mnu->lnum = 0;
  mnu->rebuild = false;

  menu_restart(mnu);
}

void menu_stop(Menu *mnu)
{
  log_msg("MENU", "menu_stop");
  werase(mnu->nc_win);
  wnoutrefresh(mnu->nc_win);

  delwin(mnu->nc_win);
  window_shift(ROW_MAX+1);
}

void menu_restart(Menu *mnu)
{
  mnu->cx = context_start();
  mnu->docmpl = false;
  ex_cmd_push(mnu->cx);
  compl_build(mnu->cx, NULL);
  compl_update(mnu->cx, "");
}

static void rebuild_contexts(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "rebuild_contexts");
  Token *word;
  int i = 0;
  int pos = 0;
  while ((word = cmdline_tokindex(cmd, i))) {
    String key = token_val(word, VAR_STRING);
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
    compl_build(mnu->cx, ex_cmd_curlist());
    compl_update(mnu->cx, ex_cmd_curstr());
  }
  mnu->rebuild = false;
}

void menu_rebuild(Menu *mnu)
{
  menu_restart(mnu);
  mnu->rebuild = true;
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
  if (mnu->docmpl || compl_isdynamic(mnu->cx))
    compl_build(mnu->cx, ex_cmd_curlist());

  if (!compl_isdynamic(mnu->cx))
    mnu->line_key = ex_cmd_curstr();

  compl_update(mnu->cx, mnu->line_key);

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

    int ofs = strlen(row->key);
    for (int c = 0; c < row->colcount; c++) {
      String col = row->columns;
      DRAW_STR(mnu, nc_win, i, ofs+3, col, col_text);
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
