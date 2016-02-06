#include "fnav/tui/menu.h"
#include "fnav/log.h"
#include "fnav/macros.h"
#include "fnav/compl.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
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
  bool active;
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
  if (!cur_menu->active)
    return;

  ventry *ent = fnd_val("fm_files", "dir", cur_menu->hndl->key);
  if (!ent)
    return;

  int count = tbl_ent_count(ent);
  compl_new(count, COMPL_DYNAMIC);

  for (int i = 0; i < count; i++) {
    fn_rec *rec = ent->rec;
    compl_set_index(i, 0, NULL, "%s", rec_fld(rec, "name"));
    ent = ent->next;
  }

  if (cur_menu->active) {
    compl_update(cur_menu->cx, cur_menu->line_key);
    window_req_draw(cur_menu, menu_queue_draw);
  }
  fs_close(cur_menu->fs);
}

void menu_ch_dir(void **args)
{
  log_msg("MENU", "menu_ch_dir");
  fs_close(cur_menu->fs);
  if (!cur_menu->active)
    return;

  String dir = args[1];
  if (!dir)
    return;

  free(cur_menu->hndl->key);
  cur_menu->hndl->key = strdup(dir);
  fs_open(cur_menu->fs, dir);
}

static int last_dir_in_path(Token *tok, List *args, int pos, String *path)
{
  String val = NULL;
  int prev = tok->block;
  int exec = 0;
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
      String tmp = conspath(*path, val);
      free(*path);
      *path = tmp;
    }
  }
  return exec;
}

static String expand_path(String line, String path)
{
  if (path[0] != '/') {
    SWAP_ALLOC_PTR(path, conspath(window_cur_dir(), path));
    cur_menu->line_key = line;
  }
  String tmp = realpath(path, NULL);
  if (!tmp)
    return path;

  if (strcmp(tmp, path)) {
    free(path);
    cur_menu->line_key = line;
    path = tmp;
  }
  else
    free(tmp);
  return path;
}

void path_list(List *args)
{
  log_msg("MENU", "path_list");
  if (!cur_menu) return;
  if (!args) {
    log_msg("ERR", "unhandled execution path");
    abort();
    return;
  }
  int pos = ex_cmd_curidx(args);
  Token *tok = tok_arg(args, pos);
  free(cur_menu->line_key);

  if (!tok) {
    String path = window_cur_dir();
    cur_menu->line_key = strdup("");
    fs_read(cur_menu->fs, path);
  }
  else {
    cur_menu->line_key = token_val(tok, VAR_STRING);
    String path = strdup(cur_menu->line_key);

    if (path[0] == '@') {
      marklbl_list(args);
      compl_update(cur_menu->cx, cur_menu->line_key);
      cur_menu->line_key = path;
      return;
    }

    path = expand_path(cur_menu->line_key, path);
    int exec = last_dir_in_path(tok, args, pos, &path);

    cur_menu->line_key = strdup(cur_menu->line_key);
    if (exec)
      fs_read(cur_menu->fs, path);

    free(path);
  }
}

Menu* menu_new()
{
  log_msg("MENU", "menu_new");
  Menu *mnu = malloc(sizeof(Menu));
  memset(mnu, 0, sizeof(Menu));
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
  log_msg("MENU", "menu_delete");
  fs_cleanup(mnu->fs);
  free(mnu->hndl);
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
  mnu->active = true;
  mnu->line_key = strdup("");

  menu_restart(mnu);
}

void menu_stop(Menu *mnu)
{
  log_msg("MENU", "menu_stop");
  werase(mnu->nc_win);
  wnoutrefresh(mnu->nc_win);

  delwin(mnu->nc_win);
  window_shift(ROW_MAX+1);
  free(mnu->line_key);
  mnu->active = false;
}

void menu_restart(Menu *mnu)
{
  log_msg("MENU", "menu_restart");
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
    if (find)
      mnu->cx = find->params[0];

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
  if (ex_cmd_state() & (EX_CYCLE|EX_QUIT))
    return;

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
    else
      mnu->cx = NULL;

    ex_cmd_push(mnu->cx);
  }

  if (mnu->docmpl || compl_isdynamic(mnu->cx))
    compl_build(mnu->cx, ex_cmd_curlist());

  String line = ex_cmd_curstr();
  if (compl_isdynamic(mnu->cx))
    line = mnu->line_key;

  compl_update(mnu->cx, line);
  mnu->docmpl = false;
}

void menu_draw(Menu *mnu)
{
  log_msg("MENU", "menu_draw");
  if (!mnu || !mnu->active)
    return;

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
  if (!mnu->cx || !mnu->cx->cmpl)
    return NULL;

  fn_compl *cmpl = mnu->cx->cmpl;
  log_msg("MENU", "%d %d", mnu->lnum, cmpl->matchcount);
  if (cmpl->matchcount < 1)
    return NULL;

  String before = cycle_matches(mnu, dir);

  if (strcmp(ex_cmd_curstr(), before) == 0)
    before = cycle_matches(mnu, dir);

  return before;
}
