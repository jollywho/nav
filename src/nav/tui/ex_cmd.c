#include "nav/tui/ex_cmd.h"
#include "nav/nav.h"
#include "nav/cmdline.h"
#include "nav/regex.h"
#include "nav/ascii.h"
#include "nav/log.h"
#include "nav/tui/layout.h"
#include "nav/tui/window.h"
#include "nav/tui/message.h"
#include "nav/tui/history.h"
#include "nav/tui/menu.h"
#include "nav/option.h"
#include "nav/event/input.h"
#include "nav/util.h"

static void cmdline_draw();
static void ex_esc();
static void ex_car();
static void ex_tab();
static void ex_bckspc();
static void ex_killword();
static void ex_killline();
static void ex_hist();
static void ex_menuhints();
static void ex_menu_mv();
static void ex_getchar();

typedef struct Ex_cmd Ex_cmd;
struct Ex_cmd {
  WINDOW *nc_win;
  int curpos;
  int maxpos;
  Cmdline cmd;
  char *line;
  Menu *menu;
  LineMatch *lm;
  Filter *fil;

  char state_symbol;
  int ex_state;
  int col_text;
  int col_symb;
  int inrstate;
};

static fn_key key_defaults[] = {
  {ESC,      ex_esc,           0,       0},
  {TAB,      ex_tab,           0,       FORWARD},
  {K_S_TAB,  ex_tab,           0,       BACKWARD},
  {CAR,      ex_car,           0,       0},
  {BS,       ex_bckspc,        0,       0},
  {Ctrl_P,   ex_hist,          0,       BACKWARD},
  {Ctrl_N,   ex_hist,          0,       FORWARD},
  {Ctrl_W,   ex_killword,      0,       0},
  {Ctrl_U,   ex_killline,      0,       0},
  {Ctrl_L,   ex_cmdinvert,     0,       0},
  {Ctrl_G,   ex_menuhints,     0,       0},
  {Ctrl_J,   ex_menu_mv,       0,       FORWARD},
  {Ctrl_K,   ex_menu_mv,       0,       BACKWARD},
};
static fn_keytbl key_tbl;
static short cmd_idx[LENGTH(key_defaults)];
static Ex_cmd ex;

void ex_cmd_init()
{
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = LENGTH(key_defaults);
  input_setup_tbl(&key_tbl);
  ex.menu = menu_new();
}

void ex_cmd_cleanup()
{
  log_msg("CLEANUP", "ex_cmd_cleanup");
  menu_delete(ex.menu);
}

void start_ex_cmd(char symbol, int state)
{
  log_msg("EXCMD", "start");
  ex.ex_state = state;
  ex.state_symbol = symbol;
  window_refresh();

  pos_T max = layout_size();
  ex.curpos = 0;
  ex.maxpos = max.col - 2;
  ex.inrstate = 0;
  ex.col_text = opt_color(COMPL_TEXT);
  ex.col_symb = opt_color(BUF_TEXT);

  switch (state) {
    case EX_CMD_STATE:
      menu_start(ex.menu);
      break;
    case EX_REG_STATE:
      ex.lm = window_get_focus()->matches;
      regex_mk_pivot(ex.lm);
      break;
    case EX_FIL_STATE:
      ex.fil = window_get_focus()->filter;
      //swap exline with filter string
      break;
  }
  ex.line = calloc(max.col, sizeof(char*));
  ex.cmd.cmds = NULL;
  ex.cmd.line = NULL;
  hist_push(state, &ex.cmd);
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  if (ex.ex_state == EX_CMD_STATE)
    menu_stop(ex.menu);
  ex.lm = NULL;
  ex.fil = NULL;
  free(ex.line);
  cmdline_cleanup(&ex.cmd);
  if (!message_pending) {
    werase(ex.nc_win);
    wnoutrefresh(ex.nc_win);
  }
  curs_set(0);
  doupdate();
  ex.ex_state = EX_OFF_STATE;
  window_ex_cmd_end();
  cmd_flush();
  window_refresh();
}

static void str_ins(char *str, const char *ins, int pos, int ofs)
{
  char *buf = strdup(str);
  strncpy(str, buf, pos);
  strcpy(str+pos, ins);
  strcpy(str+strlen(str), buf+pos+ofs);
  free(buf);
}

static void cmdline_draw()
{
  log_msg("EXCMD", "cmdline_draw");
  werase(ex.nc_win);

  if (ex.ex_state == EX_CMD_STATE)
    menu_draw(ex.menu);

  pos_T max = layout_size();
  wchar_t *wline = str2wide(ex.line);
  int len = 2 + wcswidth(wline, -1);
  int offset = MAX(len - (max.col - 1), 0);

  mvwaddch(ex.nc_win, 0, 0, ex.state_symbol);
  mvwaddwstr(ex.nc_win, 0, 1, &wline[offset]);
  mvwchgat(ex.nc_win, 0, 0, 1, A_NORMAL, ex.col_symb, NULL);
  mvwchgat(ex.nc_win, 0, 1, -1, A_NORMAL, ex.col_text, NULL);

  wmove(ex.nc_win, 0, (ex.curpos + 1) - offset);
  curs_set(1);
  wnoutrefresh(ex.nc_win);
  free(wline);
}

void cmdline_resize()
{
  pos_T max = layout_size();
  delwin(ex.nc_win);
  ex.nc_win = newwin(1, 0, max.lnum - 1, 0);
  menu_resize(ex.menu);
}

void cmdline_refresh()
{
  cmdline_draw();
}

static void ex_esc()
{
  if (ex.ex_state == EX_REG_STATE)
    regex_pivot(ex.lm);

  hist_save();
  ex.inrstate = EX_QUIT;
}

static void ex_tab(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "TAB");
  char *line = menu_next(ex.menu, arg->arg);
  if (!line)
    return;

  int st = compl_cur_pos();
  char *newline = escape_shell(line);

  int len = cell_len(newline);
  if (st + len >= ex.maxpos)
    ex.line = realloc(ex.line, ex.maxpos = (2*ex.maxpos)+len+1);

  ex.line[ex.curpos] = ' ';
  strcpy(&ex.line[st], newline);
  ex.curpos = cell_len(ex.line);
  ex.inrstate = EX_CYCLE;

  free(line);
  free(newline);
}

static void ex_hist(void *none, Keyarg *arg)
{
  const char *ret = NULL;

  if (arg->arg == BACKWARD)
    ret = hist_prev();
  if (arg->arg == FORWARD)
    ret = hist_next();
  if (ret) {
    ex_cmd_populate(ret);
    ex.inrstate = EX_HIST;
  }
}

void ex_cmd_populate(const char *newline)
{
  if (ex.ex_state == EX_CMD_STATE)
    menu_rebuild(ex.menu);

  int len = strlen(newline);
  if (strlen(newline) >= ex.maxpos)
    ex.line = realloc(ex.line, ex.maxpos = (2*ex.maxpos)+len);
  strcpy(ex.line, newline);

  ex.curpos = cell_len(ex.line);
}

static void ex_car()
{
  log_msg("EXCMD", "excar %s", ex.line);
  if (ex.ex_state == EX_CMD_STATE)
    cmd_eval(NULL, ex.line);

  hist_save();
  ex.inrstate = EX_QUIT;
}

static void ex_bckspc()
{
  log_msg("EXCMD", "bkspc");

  if (ex.curpos > 0) {
    wchar_t *tmp = str2wide(ex.line);
    tmp[wcslen(tmp)-1] = '\0';

    char *nline = wide2str(tmp);
    if (strlen(nline) >= ex.maxpos) {
      ex.line = realloc(ex.line, ex.maxpos *= 2);
      ex.line[ex.curpos+2] = '\0';
    }
    strcpy(ex.line, nline);
    ex.curpos = cell_len(ex.line);
    free(tmp);
    free(nline);
  }

  if (ex.curpos < 0)
    ex.curpos = 0;

  if (ex.ex_state == EX_CMD_STATE) {
    if (ex.curpos < compl_cur_pos())
      menu_killword(ex.menu);
  }
  ex.inrstate &= ~EX_FRESH;

  log_err("MENU", "##%d %d", ex.curpos, compl_cur_pos());
}

static void ex_killword()
{
  while (strlen(ex_cmd_curstr()) < 1) {
    menu_killword(ex.menu);
    if (compl_cur_pos() < 1)
      break;
  }

  Token *cur = ex_cmd_curtok();
  if (!cur)
    return;

  int st = cur->start;
  int ed = cur->end;
  if (ed - st > 0) {
    for (int i = st; i < ed; i++)
      ex.line[i] = '\0';
  }

  ex.curpos = st;
}

static void ex_killline()
{
  free(ex.line);
  ex.line = calloc(ex.maxpos, sizeof(char*));
  ex.curpos = 0;

  if (ex.ex_state == EX_CMD_STATE)
    menu_clear(ex.menu);

  ex.inrstate = 0;
}

void ex_cmdinvert()
{
  List *list = ex_cmd_curlist();
  if (!list || utarray_len(list->items) < 1)
    return;
  Token *word0 = tok_arg(list, 0);
  Token *word1 = cmdline_tokbtwn(&ex.cmd, word0->end, word0->end+1);
  char *excl = token_val(word1, VAR_STRING);
  if (excl && excl[0] == '!') {
    str_ins(ex.line, "", word1->start, 1);
    ex.curpos--;
  }
  else {
    str_ins(ex.line, "!", word0->end, 0);
    ex.curpos++;
  }
}

static void ex_menuhints()
{
  menu_toggle_hints(ex.menu);
  ex.inrstate = EX_CYCLE;
}

static void ex_menu_mv(void *none, Keyarg *arg)
{
  menu_mv(ex.menu, arg->arg);
  ex.inrstate = EX_CYCLE;
}

static void ex_check_pipe()
{
  if ((ex.inrstate & EX_HIST))
    return;

  Cmdstr *cur = ex_cmd_curcmd();
  if (cur && cur->exec) {
    ex.inrstate = EX_EXEC;
    return;
  }
  if (ex.inrstate & EX_EXEC) {
    menu_restart(ex.menu);
    ex.inrstate = 0;
  }
  Cmdstr *prev = ex_cmd_prevcmd();
  if (prev && prev->flag) {
    menu_restart(ex.menu);
    ex.inrstate = 0;
  }
}

static void ex_onkey()
{
  log_msg("EXCMD", "##%d", ex_cmd_state());
  cmdline_cleanup(&ex.cmd);
  cmdline_build(&ex.cmd, ex.line);

  switch (ex.ex_state) {
    case EX_CMD_STATE:
      ex_check_pipe();
      menu_update(ex.menu, &ex.cmd);
      break;
    case EX_REG_STATE:
      if (window_focus_attached() && ex.curpos > 0) {
        regex_build(ex.lm, ex.line);
        regex_hover(ex.lm);
      }
      break;
    case EX_FIL_STATE:
      if (ex.fil) {
        filter_build(ex.fil, ex.line);
        filter_update(ex.fil);
      }
      break;
  }
  ex.inrstate &= ~EX_CLEAR;
  window_req_draw(NULL, NULL);
}

static void ex_getchar(Keyarg *ca)
{
  if (IS_SPECIAL(ca->key))
    return;

  ex.inrstate &= ~(EX_FRESH);

  char instr[7] = {0,0};
  if (!ca->utf8)
    instr[0] = ca->key;
  else
    strcpy(instr, ca->utf8);

  int len = cell_len(instr);

  if ((1 + ex.curpos + len) >= ex.maxpos)
    ex.line = realloc(ex.line, ex.maxpos = (2*ex.maxpos)+len+1);

  strcat(&ex.line[ex.curpos], instr);
  ex.curpos += len;
}

void ex_input(Keyarg *ca)
{
  log_msg("EXCMD", "input");
  if (menu_hints_enabled(ex.menu)) {
    if (menu_input(ex.menu, ca->key))
      ex_car();
  }

  int idx = find_command(&key_tbl, ca->key);
  ca->arg = key_defaults[idx].cmd_arg;

  if (idx >= 0)
    key_defaults[idx].cmd_func(NULL, ca);
  else
    ex_getchar(ca);

  if (ex.inrstate & EX_QUIT)
    stop_ex_cmd();
  else
    ex_onkey();
}

char ex_cmd_curch()
{
  if (ex.curpos < 1)
    return ex.line[0];
  return ex.line[ex.curpos-1];
}

int ex_cmd_curpos()
{
  return ex.curpos;
}

Token* ex_cmd_curtok()
{
  if (!ex.cmd.cmds)
    return NULL;
  int st = compl_cur_pos();
  if (st > -1) {
    int ed = ex.curpos + 1;
    return cmdline_tokbtwn(&ex.cmd, compl_cur_pos(), ed);
  }
  st = rev_strchr_pos(ex.line, ex.curpos-1, ' ');
  int ed = ex.curpos+1;
  return cmdline_tokbtwn(&ex.cmd, st, ed);
}

char* ex_cmd_line()
{
  return ex.line;
}

char* ex_cmd_curstr()
{
  Token *tok = ex_cmd_curtok();
  if (tok)
    return token_val(tok, VAR_STRING);
  return "";
}

int ex_cmd_state()
{
  return ex.inrstate;
}

Cmdstr* ex_cmd_prevcmd()
{
  int st = compl_last_pos();
  int ed = ex.curpos + 1;
  return cmdline_cmdbtwn(&ex.cmd, st, ed);
}

Cmdstr* ex_cmd_curcmd()
{
  int st = compl_cur_pos() + 1;
  int ed = ex.curpos;
  return cmdline_cmdbtwn(&ex.cmd, st, ed);
}

List* ex_cmd_curlist()
{
  if (ex.ex_state == EX_OFF_STATE)
    return NULL;
  if (!ex.cmd.cmds)
    return NULL;

  Cmdstr *cmdstr = ex_cmd_curcmd();
  if (!cmdstr)
    cmdstr = (Cmdstr*)utarray_front(ex.cmd.cmds);
  List *list = token_val(&cmdstr->args, VAR_LIST);
  return list;
}

int ex_cmd_height()
{
  int height = 1;
  if (ex.ex_state == EX_CMD_STATE)
    height += get_opt_int("menu_rows") + 1;
  return height;
}
