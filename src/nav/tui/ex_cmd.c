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

static void ex_draw();
static void ex_esc();
static void ex_car();
static void ex_tab();
static void ex_bckspc();
static void ex_killword();
static void ex_killline();
static void ex_move();
static void ex_moveline();
static void ex_word();
static void ex_hist();
static void ex_menuhints();
static void ex_menu_mv();
static void ex_getchar();

typedef struct Ex_cmd Ex_cmd;
struct Ex_cmd {
  WINDOW *nc_win;
  int curpos;
  int maxpos;
  int curofs;
  Cmdline cmd;
  char *line;
  Menu *menu;
  LineMatch *lm;
  Filter *fil;

  char state_symbol;
  int ex_state;
  int col_text;
  int col_symb;
  int state;
};

static nv_key key_defaults[] = {
  {ESC,      ex_esc,           0,       0},
  {TAB,      ex_tab,           0,       FORWARD},
  {K_S_TAB,  ex_tab,           0,       BACKWARD},
  {CAR,      ex_car,           0,       0},
  {BS,       ex_bckspc,        0,       0},
  {K_LEFT,   ex_move,          0,       BACKWARD},
  {K_RIGHT,  ex_move,          0,       FORWARD},
  {Ctrl_S,   ex_moveline,      0,       BACKWARD},
  {Ctrl_F,   ex_moveline,      0,       FORWARD},
  {Ctrl_E,   ex_word,          0,       FORWARD},
  {Ctrl_B,   ex_word,          0,       BACKWARD},
  {Ctrl_P,   ex_hist,          0,       BACKWARD},
  {Ctrl_N,   ex_hist,          0,       FORWARD},
  {Ctrl_W,   ex_killword,      0,       0},
  {Ctrl_U,   ex_killline,      0,       0},
  {Ctrl_L,   ex_cmdinvert,     0,       0},
  {Ctrl_G,   ex_menuhints,     0,       0},
  {Ctrl_J,   ex_menu_mv,       0,       FORWARD},
  {Ctrl_K,   ex_menu_mv,       0,       BACKWARD},
};
static nv_keytbl key_tbl;
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
  ex.curofs = 0;
  ex.maxpos = max.col - 2;
  ex.state = 0;
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
  hist_push(state);
  cmdline_build(&ex.cmd, ex.line);
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
  ex.ex_state = EX_OFF_STATE;
  window_ex_cmd_end();
  cmd_flush();
  window_refresh();
}

static void str_ins(char *str, const char *ins, int pos, int ofs)
{
  char *buf = strdup(str);
  memset(str, 0, ex.maxpos);
  strncpy(str, buf, pos);
  strcpy(&str[pos], ins);

  if (pos + ofs < strlen(buf))
    strcpy(&str[strlen(str)], &buf[pos+ofs]);

  free(buf);
}

static char* linechar(int pos, int dir)
{
  char *next = NULL;
  char *prev = NULL;

  if (dir == FORWARD) {
    prev = &ex.line[pos];
    next = next_widechar(prev);
    if (!next || !prev)
      return NULL;
  }
  else if (dir == BACKWARD) {
    next = &ex.line[pos];
    prev = prev_widechar(ex.line, next);
    if (!prev)
      return NULL;
  }

  static char buf[sizeof(wchar_t)+1];
  *buf = '\0';
  strncat(buf, prev,  next - prev);
  return buf;
}

static void ex_draw()
{
  log_msg("EXCMD", "ex_draw");
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

  doupdate();
  curs_set(1);
  wmove(ex.nc_win, 0, (ex.curpos + 1) - offset);
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
  ex_draw();
}

static void ex_update()
{
  log_msg("EXCMD", "check_update");
  if ((ex.state & EX_HIST))
    return menu_update(ex.menu, &ex.cmd);

  int cur = ex.curofs - 1;
  int pos = compl_cur_pos();
  char ch = ex_cmd_curch();
  bool root = compl_isroot();

  Token *tok = ex_cmd_curtok();
  char *str = token_val(tok, VAR_STRING);
  bool quote = ((str && (*str == '\'' || *str == '\"')) || (tok && tok->quoted));

  if (ch == '|' && !quote && (pos < cur || !root)) {
    menu_restart(ex.menu);
    ex.state = 0;
  }
  else if (ch == '!' && !quote && root && pos == cur) {
    ex.state &= EX_EXEC;
    return compl_set_exec(ex.curofs);
  }
  menu_update(ex.menu, &ex.cmd);
}

static void ex_esc()
{
  if (ex.ex_state == EX_REG_STATE)
    regex_pivot(ex.lm);

  hist_save(ex.line, utarray_len(ex.cmd.tokens));
  ex.state = EX_QUIT;
}

static void ex_tab(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "TAB");
  if (ex.cmd.cmds && ex_cmd_state() & EX_EXEC)
    return;

  char *line = menu_next(ex.menu, arg->arg);
  if (!line)
    return;

  int st = compl_cur_pos() - 1;
  int ed = compl_next_pos();
  char *instr = escape_shell(line);
  int len = strlen(instr);
  int slen = strlen(ex.line);

  if (slen + len >= ex.maxpos)
    ex.line = realloc(ex.line, ex.maxpos = (2*ex.maxpos)+len);

  if (ed == -1)
    ed = slen - st;
  if (ex.line[st] == ' ')
    st++;

  str_ins(ex.line, instr, st, ed);
  //TODO: strcat trailing from ed + set next compl pos
  ex.curofs = st + len;
  ex.curpos = st + cell_len(instr);
  ex.state = EX_CYCLE;

  free(line);
  free(instr);
}

static void ex_move(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "MOVE");
  char *buf = linechar(ex.curofs, arg->arg);
  if (!buf)
    return;

  ex.curofs += arg->arg * strlen(buf);
  ex.curpos += arg->arg * cell_len(buf);

  if (ex.curofs < compl_cur_pos())
    menu_killword(ex.menu);
}

static void ex_moveline(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "MOVELINE");
  int len = arg->arg * strlen(ex.line);
  int pos = arg->arg * cell_len(ex.line);
  ex.curofs = MAX(0, len);
  ex.curpos = MAX(0, pos);

  if (arg->arg == FORWARD)
    menu_rebuild(ex.menu);
  else
    menu_clear(ex.menu);
}

static void ex_word(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "WORD");
  if (arg->arg == FORWARD) {
    char *buf = linechar(ex.curofs, FORWARD);
    int len = strlen(buf);
    if (ex.line[ex.curofs += len] != '\0')
      ex_update();

    char *fnd = strpbrk(&ex.line[ex.curofs], TOKENCHARS);
    ex.curofs = fnd ? fnd - ex.line : strlen(ex.line);
  }
  if (arg->arg == BACKWARD) {
    ex.curofs = rev_strchr_pos(ex.line, ex.curofs, TOKENCHARS);
    if (ex.curofs < compl_cur_pos())
      compl_backward();
  }
  char buf[ex.curofs];
  strncpy(buf, ex.line, ex.curofs);
  buf[ex.curofs] = '\0';
  ex.curpos = cell_len(buf);
}

static void ex_hist(void *none, Keyarg *arg)
{
  const char *ret = NULL;

  if (arg->arg == BACKWARD)
    ret = hist_prev();
  if (arg->arg == FORWARD)
    ret = hist_next();
  if (!ret)
    return;

  ex_cmd_populate(ret);
  ex.state = EX_HIST;
}

void ex_cmd_populate(const char *newline)
{
  if (ex.ex_state == EX_CMD_STATE)
    menu_rebuild(ex.menu);

  int len = strlen(newline);
  if (len >= ex.maxpos)
    ex.line = realloc(ex.line, ex.maxpos = (2*ex.maxpos)+len);

  strcpy(ex.line, newline);
  ex.curpos = cell_len(ex.line);
  ex.curofs = len;
}

static void ex_car()
{
  log_msg("EXCMD", "excar %s", ex.line);
  if (ex.ex_state == EX_CMD_STATE)
    cmd_eval(NULL, ex.line);

  hist_save(ex.line, utarray_len(ex.cmd.tokens));
  ex.state = EX_QUIT;
}

static void ex_killchar()
{
  if (ex.ex_state == EX_CMD_STATE && ex.curofs < compl_cur_pos())
    menu_killword(ex.menu);
  ex.state &= ~EX_FRESH;
}

static void ex_bckspc()
{
  log_msg("EXCMD", "bkspc");

  if (ex.curofs > 0) {
    char *buf = linechar(ex.curofs, BACKWARD);
    int len = strlen(buf);
    int clen = cell_len(buf);

    str_ins(ex.line, "", ex.curofs - len, len);

    ex.curofs -= len;
    ex.curpos -= clen;
  }
  ex_killchar();
  log_err("MENU", "##%d %d", ex.curpos, compl_cur_pos());
}

static void ex_killword()
{
  if (ex.curofs < 1)
    return;

  /* strip trailing whitespace */
  if (ex_cmd_curch() == ' ') {
    int len = 0;
    while (ex.line[ex.curofs - (len + 1)] == ' ' && ex.curofs > 1)
      len++;

    str_ins(ex.line, "", ex.curofs - len, len);
    ex.curofs -= len;
    ex.curpos -= len;
    return ex_killchar();
  }

  /* remove word from line */
  int pos = rev_strchr_pos(ex.line, ex.curofs, TOKENCHARS);
  if (strchr(TOKENCHARS, ex.line[pos]) && ex.curofs - pos > 1)
    pos++;

  int len = ex.curofs - pos;
  str_ins(ex.line, "", pos, len);
  ex.curofs = pos;

  /* find cursor position */
  char buf[pos+1];
  strncpy(buf, ex.line, ex.curofs);
  buf[ex.curofs] = '\0';
  ex.curpos = cell_len(buf);

  ex_killchar();
}

static void ex_killline()
{
  free(ex.line);
  ex.line = calloc(ex.maxpos, sizeof(char*));
  ex.curpos = 0;
  ex.curofs = 0;

  if (ex.ex_state == EX_CMD_STATE)
    menu_clear(ex.menu);

  ex.state = 0;
}

void ex_cmdinvert()
{
  int st = compl_root_pos();
  int ed = st;
  while (ex.line[ed++] == ' ');

  Token *cur = cmdline_tokbtwn(&ex.cmd, st, ed);
  Token *tok = (Token*)utarray_next(ex.cmd.tokens, cur);
  char *symb = token_val(tok, VAR_STRING);
  if (!cur)
    return;

  //TODO: rebuild compl
  if (symb && *symb == '!') {
    str_ins(ex.line, "", tok->start, 1);
    ex.curofs--, ex.curpos--;
  }
  else {
    str_ins(ex.line, "!", cur->end, 0);
    ex.curofs++, ex.curpos++;
  }
}

static void ex_menuhints()
{
  menu_toggle_hints(ex.menu);
  ex.state = EX_CYCLE;
}

static void ex_menu_mv(void *none, Keyarg *arg)
{
  menu_mv(ex.menu, arg->arg);
  ex.state = EX_CYCLE;
}

static void ex_onkey()
{
  log_msg("EXCMD", "##%d", ex_cmd_state());
  cmdline_build_tokens(&ex.cmd, ex.line);

  switch (ex.ex_state) {
    case EX_CMD_STATE:
      ex_update();
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
  ex.state &= ~EX_CLEAR;
  window_req_draw(NULL, NULL);
}

static void ex_getchar(Keyarg *ca)
{
  if (IS_SPECIAL(ca->key))
    return;

  ex.state &= ~(EX_FRESH);

  char instr[7] = {0,0};
  if (!ca->utf8)
    instr[0] = ca->key;
  else
    strcpy(instr, ca->utf8);

  int len = strlen(instr);
  if (strlen(ex.line) + len >= ex.maxpos)
    ex.line = realloc(ex.line, ex.maxpos = (2*ex.maxpos)+len);

  str_ins(ex.line, instr, ex.curofs, 0);

  ex.curpos += cell_len(instr);
  ex.curofs += len;
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

  if (ex.state & EX_QUIT)
    stop_ex_cmd();
  else
    ex_onkey();
}

char ex_cmd_curch()
{
  if (ex.curofs < 1)
    return ex.line[0];
  return ex.line[ex.curofs-1];
}

int ex_cmd_curpos()
{
  return ex.curofs;
}

Token* ex_cmd_curtok()
{
  if (!ex.cmd.cmds)
    return NULL;

  int st = compl_cur_pos() - 1;
  if (st > -1) {
    if (ex.line[st] == '|')
      st++;
    return cmdline_tokbtwn(&ex.cmd, st, ex.curofs);
  }
  st = rev_strchr_pos(ex.line, ex.curofs-1, " ");
  return cmdline_tokbtwn(&ex.cmd, st, ex.curofs);
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
  return ex.state;
}

int ex_cmd_height()
{
  int height = 1;
  if (ex.ex_state == EX_CMD_STATE)
    height += get_opt_int("menu_rows") + 1;
  return height;
}
