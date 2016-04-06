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
static void ex_spc();
static void ex_bckspc();
static void ex_killword();
static void ex_killline();
static void ex_hist();
static void ex_menuhints();
static void ex_menu_mv();

#define STACK_MIN 10

static fn_key key_defaults[] = {
  {ESC,      ex_esc,           0,       0},
  {TAB,      ex_tab,           0,       FORWARD},
  {K_S_TAB,  ex_tab,           0,       BACKWARD},
  {' ',      ex_spc,           0,       0},
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

static cmd_part **cmd_stack;
static int cur_part;
static int max_part;

static WINDOW *nc_win;
static int curpos;
static int maxpos;
static Cmdline cmd;
static char *line;
static Menu *menu;
static LineMatch *lm;

static char state_symbol;
static int ex_state;
static int col_text;
static int col_symb;
static int mflag;

void ex_cmd_init()
{
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = LENGTH(key_defaults);
  input_setup_tbl(&key_tbl);
  menu = menu_new();
}

void ex_cmd_cleanup()
{
  log_msg("CLEANUP", "ex_cmd_cleanup");
  menu_delete(menu);
}

void start_ex_cmd(char symbol, int state)
{
  log_msg("EXCMD", "start");
  ex_state = state;
  state_symbol = symbol;
  pos_T max = layout_size();
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curpos = 0;
  maxpos = max.col - 2;
  mflag = 0;
  col_text = attr_color("ComplText");
  col_symb = attr_color("BufText");

  if (state == EX_REG_STATE) {
    Buffer *buf = window_get_focus();
    lm = buf->matches;
    regex_mk_pivot(lm);
  }
  else {
    cmd_stack = malloc(STACK_MIN * sizeof(cmd_part*));
    cur_part = -1;
    max_part = STACK_MIN;
    menu_start(menu);
  }
  line = calloc(max.col, sizeof(char*));
  cmd.cmds = NULL;
  cmd.line = NULL;
  hist_push(state, &cmd);
  window_req_draw(NULL, NULL);
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  if (ex_state == EX_CMD_STATE) {
    ex_cmd_pop(-1);
    free(cmd_stack);
    menu_stop(menu);
  }
  lm = NULL;
  free(line);
  cmdline_cleanup(&cmd);
  if (!message_pending) {
    werase(nc_win);
    wnoutrefresh(nc_win);
  }
  delwin(nc_win);
  curs_set(0);
  doupdate();
  ex_state = EX_OFF_STATE;
  window_ex_cmd_end();
  cmd_flush();
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
  werase(nc_win);

  if (ex_state == EX_CMD_STATE)
    menu_draw(menu);

  pos_T max = layout_size();
  wchar_t *wline = str2wide(line);
  int len = 2 + wcswidth(wline, -1);
  int offset = MAX(len - (max.col - 1), 0);

  mvwaddch(nc_win, 0, 0, state_symbol);
  mvwaddwstr(nc_win, 0, 1, &wline[offset]);
  mvwchgat(nc_win, 0, 0, 1, A_NORMAL, col_symb, NULL);
  mvwchgat(nc_win, 0, 1, -1, A_NORMAL, col_text, NULL);

  wmove(nc_win, 0, (curpos + 1) - offset);
  doupdate();
  curs_set(1);
  wnoutrefresh(nc_win);
  free(wline);
}

void cmdline_resize()
{
  pos_T max = layout_size();
  delwin(nc_win);
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  menu_resize(menu);
}

void cmdline_refresh()
{
  cmdline_draw();
}

static void ex_esc()
{
  if (ex_state == EX_REG_STATE)
    regex_pivot(lm);

  hist_save();
  mflag = EX_QUIT;
}

static void ex_tab(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "TAB");
  char *str = menu_next(menu, arg->arg);
  if (!str)
    return;

  cmd_part *part = cmd_stack[cur_part];
  int st = part->st;

  char key[strlen(str) + 2];
  expand_escapes(key, str);
  int len = cell_len(key);
  free(str);

  if (st + len >= maxpos)
    line = realloc(line, maxpos = (2*maxpos)+len+1);

  line[curpos] = ' ';
  strcpy(&line[st], key);
  curpos = cell_len(line);
  mflag = EX_CYCLE;
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
    mflag = EX_HIST;
  }
}

void ex_cmd_populate(const char *newline)
{
  if (ex_state == EX_CMD_STATE) {
    ex_cmd_pop(-1);
    menu_rebuild(menu);
  }

  int len = strlen(newline);
  if (strlen(newline) >= maxpos)
    line = realloc(line, maxpos = (2*maxpos)+len);
  strcpy(line, newline);

  curpos = cell_len(line);
}

static void ex_car()
{
  log_msg("EXCMD", "excar %s", line);
  if (ex_state == EX_CMD_STATE) {
    cmd_eval(NULL, line);
  }

  hist_save();
  mflag = EX_QUIT;
}

static void ex_spc()
{
  log_msg("EXCMD", "ex_spc");
  mflag |= EX_RIGHT;
  mflag &= ~(EX_FRESH|EX_NEW);

  if (curpos + 1 >= maxpos) {
    line = realloc(line, maxpos *= 2);
    line[curpos+2] = '\0';
  }
  strcat(&line[curpos++], " ");
}

static void ex_bckspc()
{
  log_msg("EXCMD", "bkspc");

  if (curpos > 0) {
    wchar_t *tmp = str2wide(line);
    tmp[wcslen(tmp)-1] = '\0';

    char *nline = wide2str(tmp);
    if (strlen(nline) >= maxpos) {
      line = realloc(line, maxpos *= 2);
      line[curpos+2] = '\0';
    }
    strcpy(line, nline);
    curpos = cell_len(line);
    free(tmp);
    free(nline);
  }

  mflag |= EX_LEFT;
  if (curpos < 0)
    curpos = 0;

  if (ex_state == EX_CMD_STATE) {
    if (curpos < cmd_stack[cur_part]->st)
      mflag |= EX_EMPTY;
  }
  mflag &= ~EX_FRESH;
}

static void ex_killword()
{
  //FIXME: regex_state does not build cmdline
  Token *last = cmdline_last(&cmd);
  if (!last)
    return;

  int st = last->start;
  int ed = last->end;
  for (int i = st; i < ed; i++)
    line[i] = '\0';

  curpos = st;
  ex_bckspc();
}

static void ex_killline()
{
  free(line);
  line = calloc(maxpos, sizeof(char*));
  curpos = 0;

  if (ex_state == EX_CMD_STATE) {
    ex_cmd_pop(-1);
    menu_restart(menu);
  }
  mflag = 0;
}

void ex_cmdinvert()
{
  List *list = ex_cmd_curlist();
  if (!list || utarray_len(list->items) < 1)
    return;
  Token *word0 = tok_arg(list, 0);
  Token *word1 = cmdline_tokbtwn(&cmd, word0->end, word0->end+1);
  char *excl = token_val(word1, VAR_STRING);
  if (excl && excl[0] == '!') {
    str_ins(line, "", word1->start, 1);
    curpos--;
  }
  else {
    str_ins(line, "!", word0->end, 0);
    curpos++;
  }
}

static void ex_menuhints()
{
  menu_toggle_hints(menu);
}

static void ex_menu_mv(void *none, Keyarg *arg)
{
  menu_mv(menu, arg->arg);
  mflag = EX_CYCLE;
}

static void ex_check_pipe()
{
  Cmdstr *cur = ex_cmd_curcmd();
  if (cur && cur->exec) {
    mflag = EX_EXEC;
    return;
  }
  if (mflag & EX_EXEC) {
    ex_cmd_pop(-1);
    menu_restart(menu);
    mflag = 0;
  }
  Cmdstr *prev = ex_cmd_prevcmd();
  if (prev && prev->flag) {
    menu_restart(menu);
    mflag = 0;
  }
}

static void check_new_state()
{
  if ((mflag & (EX_FRESH|EX_HIST)))
    return;
  ex_check_pipe();

  Token *tok = cmdline_last(&cmd);
  if (!tok)
    return;
  if (curpos > tok->end && curpos > 0)
    mflag |= EX_NEW;
}

static void ex_onkey()
{
  log_msg("EXCMD", "##%d", ex_cmd_state());
  if (ex_state == EX_CMD_STATE) {
    cmdline_cleanup(&cmd);
    cmdline_build(&cmd, line);
    check_new_state();
    menu_update(menu, &cmd);
  }
  else {
    if (window_focus_attached() && curpos > 0) {
      regex_build(lm, line);
      regex_pivot(lm);
      regex_hover(lm);
    }
  }
  mflag &= ~EX_CLEAR;
  window_req_draw(NULL, NULL);
}

void ex_input(int key, char utf8[7])
{
  log_msg("EXCMD", "input");
  if (menu_hints_enabled(menu))
    return menu_input(menu, key);

  Keyarg ca;
  int idx = find_command(&key_tbl, key);
  ca.arg = key_defaults[idx].cmd_arg;
  if (idx >= 0)
    key_defaults[idx].cmd_func(NULL, &ca);
  else {
    mflag |= EX_RIGHT;
    mflag &= ~(EX_FRESH|EX_NEW);

    char instr[7] = {0,0};
    if (!utf8)
      instr[0] = key;
    else
      strcpy(instr, utf8);

    int len = cell_len(instr);

    if ((1 + curpos + len) >= maxpos)
      line = realloc(line, maxpos = (2*maxpos)+len+1);

    strcat(&line[curpos], instr);
    curpos += len;
  }

  if (mflag & EX_QUIT)
    stop_ex_cmd();
  else
    ex_onkey();
}

void ex_cmd_push(fn_context *cx, int *save)
{
  log_msg("EXCMD", "ex_cmd_push");
  cur_part++;

  if (cur_part >= max_part) {
    max_part *= 2;
    cmd_stack = realloc(cmd_stack, max_part*sizeof(cmd_part*));
  }

  cmd_stack[cur_part] = malloc(sizeof(cmd_part));
  cmd_stack[cur_part]->cx = cx;

  int st = 0;
  if (cur_part > 0) {
    st = curpos;
    if (line[st] == '|')
      st++;
  }
  if (save)
    st = *save;
  cmd_stack[cur_part]->st = st;
  mflag &= ~EX_NEW;
  mflag |= EX_FRESH;
}

cmd_part* ex_cmd_pop(int count)
{
  log_msg("EXCMD", "ex_cmd_pop");
  if (cur_part < 0)
    return NULL;

  if (cur_part - count < 0)
    count = cur_part;
  else if (count == -1)
    count = cur_part + 1;

  while (count > 0) {
    compl_destroy(cmd_stack[cur_part]->cx);
    free(cmd_stack[cur_part]);
    cur_part--;
    count--;
  }
  if (cur_part < 0)
    return NULL;
  return cmd_stack[cur_part];
}

char ex_cmd_curch()
{
  return line[curpos];
}

int ex_cmd_curpos()
{
  return curpos;
}

Token* ex_cmd_curtok()
{
  cmd_part *part = cmd_stack[cur_part];
  int st = part->st;
  int ed = curpos + 1;
  if (!cmd.cmds)
    return NULL;
  Token *tok = cmdline_tokbtwn(&cmd, st, ed);
  return tok;
}

char* ex_cmd_line()
{
  return line;
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
  return mflag;
}

Cmdstr* ex_cmd_prevcmd()
{
  if (cur_part < 1)
    return NULL;
  cmd_part *part = cmd_stack[cur_part - 1];
  int st = part->st;
  int ed = curpos + 1;
  return cmdline_cmdbtwn(&cmd, st, ed);
}

Cmdstr* ex_cmd_curcmd()
{
  cmd_part *part = cmd_stack[cur_part];
  int st = part->st + 1;
  int ed = curpos;
  return cmdline_cmdbtwn(&cmd, st, ed);
}

List* ex_cmd_curlist()
{
  if (ex_state == EX_OFF_STATE)
    return NULL;
  if (!cmd.cmds)
    return NULL;

  Cmdstr *cmdstr = ex_cmd_curcmd();
  if (!cmdstr)
    cmdstr = (Cmdstr*)utarray_front(cmd.cmds);
  List *list = token_val(&cmdstr->args, VAR_LIST);
  return list;
}

int ex_cmd_curidx(List *list)
{
  cmd_part *part = cmd_stack[cur_part];
  int st = part->st;
  int ed = curpos + 1;
  return utarray_eltidx(list->items, list_tokbtwn(list, st, ed));
}
