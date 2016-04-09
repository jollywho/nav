#include "nav/tui/menu.h"
#include "nav/log.h"
#include "nav/macros.h"
#include "nav/compl.h"
#include "nav/tui/ex_cmd.h"
#include "nav/tui/layout.h"
#include "nav/tui/window.h"
#include "nav/option.h"
#include "nav/event/fs.h"
#include "nav/info.h"
#include "nav/ascii.h"
#include "nav/util.h"

static int ROW_MAX;
static Menu *cur_menu;

struct Menu {
  WINDOW *nc_win;
  fn_handle *hndl;
  fn_fs *fs;

  fn_context *cx;
  bool docmpl;
  bool rebuild;
  bool active;
  bool hints;
  bool moved;
  char *line_key;
  char *line_path;
  char *hintkeys;

  pos_T size;
  pos_T ofs;

  int top;
  int lnum;

  int col_text;
  int col_div;
  int col_select;
  int col_box;
  int col_line;
  int col_sel;
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
    compl_set_key(i, "%s", rec_fld(rec, "name"));
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

  char *dir = args[1];
  if (!dir)
    return;

  SWAP_ALLOC_PTR(cur_menu->hndl->key, strdup(dir));
  fs_open(cur_menu->fs, dir);
}

static int last_dir_in_path(Token *tok, List *args, int pos, char **path)
{
  char *val = NULL;
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
      char *tmp = conspath(*path, val);
      free(*path);
      *path = tmp;
    }
  }
  return exec;
}

static int expand_path(char *line, char **path)
{
  char *dir = valid_full_path(window_cur_dir(), *path);
  if (!dir)
    return 0;

  if (strcmp(dir, *path)) {
    free(*path);
    cur_menu->line_key = line;
    *path = dir;
  }
  else
    free(dir);

  return 1;
}

void path_list(List *args)
{
  log_msg("MENU", "path_list");
  if (!cur_menu)
    return;
  if (!args) {
    log_msg("ERR", "unhandled execution path");
    abort();
    return;
  }
  int pos = ex_cmd_curidx(args);
  Token *tok = tok_arg(args, pos);
  free(cur_menu->line_key);

  if (!tok) {
    char *path = window_cur_dir();
    cur_menu->line_key = strdup("");
    fs_read(cur_menu->fs, path);
  }
  else {
    cur_menu->line_key = token_val(tok, VAR_STRING);
    char *path = strdup(cur_menu->line_key);

    if (path[0] == '@') {
      marklbl_list(args);
      compl_update(cur_menu->cx, cur_menu->line_key);
      cur_menu->line_key = path;
      return;
    }

    int exec = expand_path(cur_menu->line_key, &path);
    if (exec)
      exec = last_dir_in_path(tok, args, pos, &path);

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
  free(mnu->hndl->key);
  free(mnu->hndl);
  free(mnu);
  cur_menu = NULL;
}

void menu_start(Menu *mnu)
{
  log_msg("MENU", "menu_start");

  mnu->col_select = attr_color("BufSelected");
  mnu->col_sel    = attr_color("ComplSelected");
  mnu->col_text   = attr_color("ComplText");
  mnu->col_div    = attr_color("OverlaySep");
  mnu->col_box    = attr_color("OverlayActive");
  mnu->col_line   = attr_color("OverlayLine");
  ROW_MAX = get_opt_int("menu_rows");
  mnu->hintkeys = get_opt_str("hintkeys");

  mnu->lnum = 0;
  mnu->rebuild = false;
  mnu->active = true;
  mnu->line_key = strdup("");

  menu_resize(mnu);
  menu_restart(mnu);
}

void menu_stop(Menu *mnu)
{
  log_msg("MENU", "menu_stop");
  werase(mnu->nc_win);
  wnoutrefresh(mnu->nc_win);

  delwin(mnu->nc_win);
  mnu->nc_win = NULL;

  free(mnu->line_key);
  free(mnu->hndl->key);
  mnu->hndl->key = NULL;
  mnu->active = false;
  mnu->hints = false;
  mnu->moved = true;
}

void menu_restart(Menu *mnu)
{
  log_msg("MENU", "menu_restart");
  mnu->cx = context_start();
  mnu->docmpl = false;
  mnu->moved = true;
  mnu->top = 0;
  free(mnu->hndl->key);
  mnu->hndl->key = NULL;

  ex_cmd_push(mnu->cx, 0);
  compl_build(mnu->cx, NULL);
  compl_update(mnu->cx, "");
}

static void rebuild_contexts(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "rebuild_contexts");
  Token *word;
  int i = 0;
  while ((word = cmdline_tokindex(cmd, i))) {
    char *key = token_val(word, VAR_STRING);
    fn_context *find = find_context(mnu->cx, key);
    if (find)
      mnu->cx = find->params[0];

    ex_cmd_push(mnu->cx, &word->end);
    i++;
  }
  if (i > 0) {
    mnu->cx = ex_cmd_pop(1)->cx;
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

static void mnu_scroll(Menu *mnu, int y, int max)
{
  int prev = mnu->top;
  mnu->top += y;

  if (mnu->top + ROW_MAX > max)
    mnu->top = max - ROW_MAX;
  if (mnu->top < 0)
    mnu->top = 0;

  int diff = prev - mnu->top;
  mnu->lnum += diff;
}

static char* cycle_matches(Menu *mnu, int dir, int mov)
{
  if (mnu->moved)
    dir = 0;

  fn_compl *cmpl = mnu->cx->cmpl;
  mnu->lnum += dir;

  if (dir < 0 && mnu->lnum < ROW_MAX * 0.2)
    mnu_scroll(mnu, dir, cmpl->matchcount);
  else if (dir > 0 && mnu->lnum > ROW_MAX * 0.8)
    mnu_scroll(mnu, dir, cmpl->matchcount);

  if (mnu->lnum < 0)
    mnu->lnum = 0;
  if (mnu->lnum > ROW_MAX)
    mnu->lnum = ROW_MAX;

  int idx = mnu->top + mnu->lnum;
  if (idx > cmpl->matchcount - 1) {
    int count = cmpl->matchcount - mnu->top;
    mnu->lnum = count - 1;
    idx = mnu->top + mnu->lnum;
  }

  char *before = cmpl->matches[idx]->key;
  mnu->moved = mov;

  return before;
}

char* menu_next(Menu *mnu, int dir)
{
  log_msg("MENU", "menu_next");
  if (!mnu->cx || !mnu->cx->cmpl)
    return NULL;

  fn_compl *cmpl = mnu->cx->cmpl;
  if (cmpl->matchcount < 1)
    return NULL;

  bool wasmoved = mnu->moved;
  char *line = cycle_matches(mnu, dir, false);

  if (!strcmp(ex_cmd_curstr(), line) && !wasmoved) {
    if (mnu->top + mnu->lnum == 0) {
      mnu->lnum = cmpl->matchcount;
      mnu->top = MAX(0, cmpl->matchcount - ROW_MAX);
    }
    else {
      mnu->lnum = 0;
      mnu->top = 0;
    }
    line = cycle_matches(mnu, 0, false);
  }

  if (cur_menu->hndl->key && !mark_path(line))
    line = conspath(cur_menu->hndl->key, line);
  else
    line = strdup(line);

  return line;
}

void menu_mv(Menu *mnu, int y)
{
  log_msg("MENU", "menu_mv");
  if (!mnu->cx || !mnu->cx->cmpl || mnu->cx->cmpl->matchcount < 1)
    return;
  mnu->moved = false;
  cycle_matches(mnu, y, true);
}

bool menu_hints_enabled(Menu *mnu)
{ return mnu->hints; }

void menu_toggle_hints(Menu *mnu)
{
  log_msg("MENU", "toggle hints");
  if (strlen(mnu->hintkeys) < ROW_MAX) {
    log_err("MENU", "not enough hintkeys for menu rows!");
    return;
  }
  mnu->hints = !mnu->hints;
  if (!mnu->cx || !mnu->cx->cmpl || ex_cmd_state() & EX_EXEC)
    mnu->hints = false;
}

void menu_input(Menu *mnu, int key)
{
  log_msg("MENU", "toggle hints");

  window_req_draw(cur_menu, menu_queue_draw);
  if (key == ESC)
    return menu_toggle_hints(mnu);
  if (key == Ctrl_L)
    ex_cmdinvert();

  char *str = NULL;

  fn_compl *cmpl = mnu->cx->cmpl;
  for (int i = 0; i < cmpl->matchcount; i++) {
    if (mnu->hintkeys[i] == key)
      str = cmpl->matches[i]->key;
  }
  if (!str)
    return;

  int pos = ex_cmd_curpos();
  Token *cur = ex_cmd_curtok();
  if (cur)
    pos = cur->start;

  char newline[pos + strlen(str)];
  strcpy(newline, ex_cmd_line());
  strcpy(&newline[pos], str);
  ex_cmd_populate(newline);
  menu_toggle_hints(mnu);
  ex_input(CAR, 0);
}

void menu_update(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "menu_update");
  log_msg("MENU", "##%d", ex_cmd_state());

  if (mnu->rebuild)
    return rebuild_contexts(mnu, cmd);
  if (ex_cmd_state() & (EX_CYCLE|EX_QUIT|EX_EXEC))
    return;

  mnu->top = 0;
  mnu->lnum = 0;
  mnu->moved = true;

  if ((ex_cmd_state() & EX_POP) == EX_POP) {
    mnu->cx = ex_cmd_pop(1)->cx;
    mnu->docmpl = true;
  }
  else if ((ex_cmd_state() & EX_PUSH) == EX_PUSH) {
    char *key = ex_cmd_curstr();

    fn_context *find = find_context(mnu->cx, key);
    if (find) {
      mnu->cx = find->params[0];
      mnu->docmpl = true;
    }
    else
      mnu->cx = NULL;

    ex_cmd_push(mnu->cx, 0);
  }

  if (mnu->docmpl || compl_isdynamic(mnu->cx))
    compl_build(mnu->cx, ex_cmd_curlist());

  char *line = ex_cmd_curstr();
  if (compl_isdynamic(mnu->cx))
    line = mnu->line_key;

  compl_update(mnu->cx, line);
  mnu->docmpl = false;
}

void menu_resize(Menu *mnu)
{
  log_msg("MENU", "menu_resize");
  if (!mnu->active)
    return;

  pos_T size = layout_size();
  mnu->size = (pos_T){ROW_MAX+1, size.col};
  mnu->ofs =  (pos_T){size.lnum - (ROW_MAX+2), 0};

  delwin(mnu->nc_win);
  mnu->nc_win = newwin(
      mnu->size.lnum,
      mnu->size.col,
      mnu->ofs.lnum,
      mnu->ofs.col);
}

void menu_draw(Menu *mnu)
{
  log_msg("MENU", "menu_draw");
  if (!mnu || !mnu->active)
    return;

  werase(mnu->nc_win);

  if (!mnu->cx || !mnu->cx->cmpl || ex_cmd_state() & EX_EXEC) {
    wnoutrefresh(mnu->nc_win);
    return;
  }
  fn_compl *cmpl = mnu->cx->cmpl;

  int colmax = mnu->size.col - 3;

  for (int i = 0; i < MIN(ROW_MAX, cmpl->matchcount); i++) {
    compl_item *row = cmpl->matches[mnu->top + i];

    /* draw hints */
    if (mnu->hints)
      DRAW_CH(mnu, nc_win, i, 0, mnu->hintkeys[i], col_div);
    else
      DRAW_CH(mnu, nc_win, i, 0, '>', col_div);

    /* draw compl key */
    draw_wide(mnu->nc_win, i, 2, row->key, colmax);

    /* draw extra column */
    int ofs = strlen(row->key);
    for (int c = 0; c < row->colcount; c++) {
      char *col = row->columns;
      draw_wide(mnu->nc_win, i, ofs+3, col, colmax);
    }
  }
  char *context = mnu->cx->key;
  draw_wide(mnu->nc_win, ROW_MAX, 1, context, mnu->size.col);
  mvwchgat(mnu->nc_win, ROW_MAX, 0, -1, A_NORMAL, mnu->col_line, NULL);

  mvwchgat(mnu->nc_win, mnu->lnum, 0, -1, A_NORMAL, mnu->col_sel, NULL);
  wnoutrefresh(mnu->nc_win);
}
