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
  Handle *hndl;
  nv_fs *fs;

  bool rebuild;
  bool active;
  bool hints;
  bool moved;
  char *line_path;
  char *hintkeys;

  pos_T size;
  pos_T ofs;

  int top;
  int lnum;

  int col_text;
  int col_div;
  int col_box;
  int col_line;
  int col_key;
  int col_sel;
  int col_param;
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

  Ventry *ent = fnd_val("fm_files", "dir", cur_menu->hndl->key);
  if (!ent)
    return;

  for (int i = 0; i < tbl_ent_count(ent); i++) {
    TblRec *rec = ent->rec;
    compl_list_add("%s", rec_fld(rec, "name"));
    ent = ent->next;
  }

  if (cur_menu->active) {
    compl_filter(ex_cmd_curstr());
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
  uv_stat_t *stat = args[2];
  if (!dir || !stat || !S_ISDIR(stat->st_mode)) {
    compl_invalidate(ex_cmd_curpos());
    return;
  }

  SWAP_ALLOC_PTR(cur_menu->hndl->key, strdup(dir));
  fs_open(cur_menu->fs, dir);
}

static int expand_path(char **path)
{
  char *dir = valid_full_path(window_cur_dir(), *path);
  if (!dir)
    return 0;

  if (strcmp(dir, *path)) {
    free(*path);
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
    return abort();
  }

  int pos = compl_arg_pos();
  int len = ex_cmd_curpos() - pos;

  compl_set_escapes("@/");

  if (pos >= ex_cmd_curpos() || len < 1) {
    char *path = window_cur_dir();
    return fs_read(cur_menu->fs, path);
  }

  char *path = &ex_cmd_line()[compl_arg_pos()];
  log_msg("MENU", "path %s", path);

  if (path[0] == '@') {
    marklbl_list(args);
    compl_filter(ex_cmd_curstr());
    return;
  }

  path = strdup(path);
  int exec = expand_path(&path);

  if (compl_validate(ex_cmd_curpos()) || ex_cmd_curch() != '/') {
    exec = true;
    char *parent = fs_parent_dir(path);
    if (*parent == '.')
      parent = window_cur_dir();

    SWAP_ALLOC_PTR(path, strdup(parent));
  }
  if (exec)
    fs_read(cur_menu->fs, path);

  free(path);
}

Menu* menu_new()
{
  log_msg("MENU", "menu_new");
  Menu *mnu = calloc(1, sizeof(Menu));
  Handle *hndl = calloc(1, sizeof(Handle));
  mnu->hndl = hndl;

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

  mnu->col_sel    = opt_color(COMPL_SELECTED);
  mnu->col_text   = opt_color(COMPL_TEXT);
  mnu->col_param  = opt_color(COMPL_PARAM);
  mnu->col_div    = opt_color(OVERLAY_SEP);
  mnu->col_box    = opt_color(OVERLAY_ACTIVE);
  mnu->col_line   = opt_color(OVERLAY_LINE);
  mnu->col_key    = opt_color(BUF_DIR);
  ROW_MAX = get_opt_int("menu_rows");
  mnu->hintkeys = get_opt_str("hintkeys");


  mnu->lnum = 0;
  mnu->active = true;

  menu_restart(mnu);
  menu_resize(mnu);
}

void menu_stop(Menu *mnu)
{
  log_msg("MENU", "menu_stop");
  werase(mnu->nc_win);
  wnoutrefresh(mnu->nc_win);
  compl_end();

  delwin(mnu->nc_win);
  mnu->nc_win = NULL;

  free(mnu->hndl->key);
  mnu->hndl->key = NULL;
  mnu->active = false;
  mnu->hints = false;
  mnu->moved = true;
}

void menu_clear(Menu *mnu)
{
  log_msg("MENU", "menu_clear");
  compl_end();
  menu_restart(mnu);
}

void menu_restart(Menu *mnu)
{
  log_msg("MENU", "menu_restart");
  compl_begin(ex_cmd_curpos());
  mnu->moved = true;
  mnu->top = 0;
  free(mnu->hndl->key);
  mnu->hndl->key = NULL;
}

void menu_killword(Menu *mnu)
{
  if (mnu->active)
    compl_backward();
}

static void rebuild_contexts(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "rebuild_contexts");
  int len = utarray_len(cmd->tokens);

  for (int i = 0; i < len; i++) {
    Token *word  = (Token*)utarray_eltptr(cmd->tokens, i);
    Token *after = (Token*)utarray_eltptr(cmd->tokens, i+1);
    char *key = token_val(word, VAR_STRING);
    char *next = token_val(after, VAR_STRING);

    /* simulate forward compl pushes */
    if (!next || !strchr(TOKENCHARS, *next))
      next = " ";

    if (*next == '/') {
      compl_set_escapes("/");
      compl_update(key, word->end+1, '/');
      i++;
    }
    else if (*key == '|')
      compl_begin(word->end+1);
    else if (*key == '!' && compl_isroot())
      compl_set_exec(word->end+1);
    else
      compl_update(key, word->end+1, *next);
  }

  /* pop last compl to maintain forward simulation */
  if (compl_cur_pos() > ex_cmd_curpos() && !compl_isexec() && !compl_isroot())
    compl_backward();

  compl_build(ex_cmd_curlist());
  compl_filter(ex_cmd_curstr());
  mnu->rebuild = false;
}

void menu_update(Menu *mnu, Cmdline *cmd)
{
  log_msg("MENU", "menu_update");

  int exstate = ex_cmd_state();

  if (mnu->rebuild)
    return rebuild_contexts(mnu, cmd);
  if (exstate & (EX_CYCLE|EX_QUIT|EX_EXEC))
    return;

  mnu->top = 0;
  mnu->lnum = 0;
  mnu->moved = true;

  compl_update(ex_cmd_curstr(), ex_cmd_curpos(), ex_cmd_curch());
  compl_build(ex_cmd_curlist());
  compl_filter(ex_cmd_curstr());
}

void menu_rebuild(Menu *mnu)
{
  menu_clear(mnu);
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

  compl_list *cmplist = compl_complist();
  mnu->lnum += dir;

  if (dir < 0 && mnu->lnum < ROW_MAX * 0.2)
    mnu_scroll(mnu, dir, cmplist->matchcount);
  else if (dir > 0 && mnu->lnum > ROW_MAX * 0.8)
    mnu_scroll(mnu, dir, cmplist->matchcount);

  if (mnu->lnum < 0)
    mnu->lnum = 0;
  if (mnu->lnum > ROW_MAX)
    mnu->lnum = ROW_MAX;

  int idx = mnu->top + mnu->lnum;
  if (idx > cmplist->matchcount - 1) {
    int count = cmplist->matchcount - mnu->top;
    mnu->lnum = count - 1;
    idx = mnu->top + mnu->lnum;
  }

  char *before = compl_idx_match(idx)->key;
  mnu->moved = mov;

  return before;
}

char* menu_next(Menu *mnu, int dir)
{
  log_msg("MENU", "menu_next");
  compl_list *cmplist = compl_complist();
  if (compl_dead() || !cmplist || cmplist->matchcount < 1)
    return NULL;

  bool wasmoved = mnu->moved;
  char *line = cycle_matches(mnu, dir, false);

  if (!strcmp(ex_cmd_curstr(), line) && !wasmoved) {
    if (mnu->top + mnu->lnum == 0) {
      mnu->lnum = cmplist->matchcount;
      mnu->top = MAX(0, cmplist->matchcount - ROW_MAX);
    }
    else {
      mnu->lnum = 0;
      mnu->top = 0;
    }
    line = cycle_matches(mnu, 0, false);
  }

  line = strdup(line);

  return line;
}

void menu_mv(Menu *mnu, int y)
{
  log_msg("MENU", "menu_mv");
  compl_list *cmplist = compl_complist();
  if (compl_dead() || !cmplist || cmplist->matchcount < 1)
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

  compl_list *cmplist = compl_complist();
  if (cmplist->matchcount < 1)
    return;

  mnu->hints = !mnu->hints;
  if (compl_dead() || !cmplist || ex_cmd_state() & EX_EXEC)
    mnu->hints = false;
}

int menu_input(Menu *mnu, int key)
{
  log_msg("MENU", "menu_input");
  if (!mnu->active || compl_dead())
    return 0;

  compl_item *item = NULL;
  compl_list *cmplist = compl_complist();
  for (int i = 0; i < MIN(ROW_MAX, cmplist->matchcount); i++) {
    if (mnu->hintkeys[i] == key)
      item = compl_idx_match(i+mnu->top);
  }

  if (key == CAR) {
    int idx = mnu->top + mnu->lnum;
    if (idx < cmplist->matchcount)
      item = compl_idx_match(idx);
  }
  if (!item)
    return 0;

  int pos = ex_cmd_curpos();
  Token *cur = ex_cmd_curtok();
  if (cur)
    pos = cur->start;

  char *str = item->key;
  char newline[pos + strlen(str)];
  strcpy(newline, ex_cmd_line());
  strcpy(&newline[pos], str);
  ex_cmd_populate(newline);
  menu_toggle_hints(mnu);
  return 1;
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

static void draw_matches(Menu *mnu, compl_list *cmplist)
{
  int colmax = mnu->size.col - 3;
  for (int i = 0; i < MIN(ROW_MAX, cmplist->matchcount); i++) {
    compl_item *row = compl_idx_match(mnu->top + i);

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
      mvwchgat(mnu->nc_win,  i, ofs+3, strlen(col), A_NORMAL, mnu->col_key, NULL);
    }
  }
}

static int draw_param(char *label, char flag, int prev, bool active)
{
  Menu *mnu = cur_menu;
  if (flag == '-')
    DRAW_CH(mnu, nc_win, ROW_MAX, prev++, '[', col_line);

  int len = strlen(label);
  draw_wide(mnu->nc_win, ROW_MAX, prev, label, mnu->size.col);

  attr_t attr = active ? A_REVERSE : A_NORMAL;
  mvwchgat(mnu->nc_win, ROW_MAX, prev, len, attr, mnu->col_param, NULL);

  if (flag == '-')
    DRAW_CH(mnu, nc_win, ROW_MAX, (prev++)+len, ']', col_line);

  return prev + len + 1;
}

void menu_draw(Menu *mnu)
{
  log_msg("MENU", "menu_draw");
  if (!mnu || !mnu->active)
    return;

  werase(mnu->nc_win);

  compl_list *cmplist = compl_complist();
  if (compl_dead() || !cmplist || ex_cmd_state() & EX_EXEC) {
    wnoutrefresh(mnu->nc_win);
    return;
  }

  draw_matches(mnu, cmplist);
  mvwchgat(mnu->nc_win, ROW_MAX,   0, -1, A_NORMAL, mnu->col_line, NULL);
  mvwchgat(mnu->nc_win, mnu->lnum, 0, -1, A_NORMAL, mnu->col_sel, NULL);

  compl_walk_params(draw_param);
  wnoutrefresh(mnu->nc_win);
}
