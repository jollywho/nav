#include "fnav/tui/ex_cmd.h"
#include "fnav/fnav.h"
#include "fnav/cmdline.h"
#include "fnav/regex.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
#include "fnav/tui/history.h"
#include "fnav/tui/menu.h"
#include "fnav/option.h"
#include "fnav/event/input.h"

static void cmdline_draw();
static void ex_esc();
static void ex_car();
static void ex_tab();
static void ex_spc();
static void ex_bckspc();
static void ex_killword();
static void ex_killline();
static void ex_hist();

#define STACK_MIN 10
#define CURS_MIN -1
#define EXCMD_HIST() ((ex_state) ? (hist_cmds) : (hist_regs))

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
static String fmt_out;
static Menu *menu;
static fn_hist *hist_cmds;
static fn_hist *hist_regs;
static LineMatch *lm;

static const char state_symbol[] = {'/',':'};
static int ex_state;
static int col_text;
static int mflag;

void ex_cmd_init()
{
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = LENGTH(key_defaults);
  input_setup_tbl(&key_tbl);
  hist_cmds = hist_new();
  hist_regs = hist_new();
  menu = menu_new();
}

void ex_cmd_cleanup()
{
  log_msg("CLEANUP", "ex_cmd_cleanup");
  hist_delete(hist_cmds);
  hist_delete(hist_regs);
  menu_delete(menu);
}

void start_ex_cmd(int state)
{
  log_msg("EXCMD", "start");
  ex_state = state;
  pos_T max = layout_size();
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curpos = CURS_MIN;
  maxpos = max.col;
  mflag = 0;
  fmt_out = malloc(1);
  col_text = attr_color("ComplText");

  if (window_get_focus() && state == EX_REG_STATE) {
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
  cmdline_init(&cmd, max.col);
  hist_push(EXCMD_HIST(), &cmd);
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
  free(cmd.line);
  free(fmt_out);
  cmdline_cleanup(&cmd);
  werase(nc_win);
  wnoutrefresh(nc_win);
  delwin(nc_win);
  curs_set(0);
  doupdate();
  ex_state = EX_OFF_STATE;
  window_ex_cmd_end();
}

static void gen_output_str()
{
  char ch;
  int i, j;

  free(fmt_out);
  fmt_out = calloc(maxpos+1, sizeof(char*));

  i = j = 0;
  while ((ch = cmd.line[i])) {
    fmt_out[j] = ch;

    if (ch == '%') {
      j++;
      fmt_out[j] = ch;
    }
    i++; j++;
  }
}

static void cmdline_draw()
{
  log_msg("EXCMD", "cmdline_draw");
  werase(nc_win);

  if (ex_state == EX_CMD_STATE)
    menu_draw(menu);

  wattron(nc_win, COLOR_PAIR(col_text));
  mvwaddch(nc_win, 0, 0, state_symbol[ex_state]);
  gen_output_str();
  mvwprintw(nc_win, 0, 1, fmt_out);
  wattroff(nc_win, COLOR_PAIR(col_text));

  wmove(nc_win, 0, curpos + 2);
  doupdate();
  curs_set(1);
  wnoutrefresh(nc_win);
}

void cmdline_refresh()
{
  pos_T max = layout_size();
  delwin(nc_win);
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  maxpos = max.col;
  cmdline_draw();
}

static void ex_esc()
{
  if (ex_state == EX_REG_STATE)
    regex_pivot(lm);

  hist_save(EXCMD_HIST());
  mflag = EX_QUIT;
}

static void ex_tab(void *none, Keyarg *arg)
{
  log_msg("EXCMD", "TAB");
  String key = menu_next(menu, arg->arg);
  if (!key)
    return;

  int st = curpos + 1;
  int ed = strlen(ex_cmd_curstr());
  Token *tok = cmdline_tokbtwn(&cmd, curpos, curpos + 1);
  if (tok) {
    st = tok->start;
    ed = tok->end;
  }

  int len = ed - st;
  if (curpos + len + 2 >= maxpos)
    cmd.line = realloc(cmd.line, maxpos *= 2);

  int i;
  for (i = st; i < ed + 2; i++)
    cmd.line[i] = ' ';
  for (i = 0; i < strlen(key); i++)
    cmd.line[st + i] = key[i];

  curpos = st + i - 1;
  mflag = EX_CYCLE;
}

static void ex_hist(void *none, Keyarg *arg)
{
  String ret = NULL;

  if (arg->arg == BACKWARD)
    ret = hist_prev(EXCMD_HIST());
  if (arg->arg == FORWARD)
    ret = hist_next(EXCMD_HIST());
  if (ret) {
    if (ex_state == EX_CMD_STATE) {
      ex_cmd_pop(-1);
      menu_rebuild(menu);
    }
    free(cmd.line);

    if (strlen(ret) < 1)
      cmd.line = calloc(maxpos, sizeof(char*));
    else
      cmd.line = strdup(ret);

    curpos = strlen(cmd.line) - 1;
    mflag = EX_HIST;
  }
}

static void ex_car()
{
  if (ex_state == EX_CMD_STATE)
    cmdline_req_run(&cmd);

  hist_save(EXCMD_HIST());
  mflag = EX_QUIT;
}

static void ex_spc()
{
  log_msg("EXCMD", "ex_spc");
  curpos++;
  mflag |= EX_RIGHT;
  mflag &= ~(EX_FRESH|EX_NEW);
  if (curpos >= maxpos)
    cmd.line = realloc(cmd.line, maxpos *= 2);

  cmd.line[curpos] = ' ';
}

static void ex_bckspc()
{
  log_msg("EXCMD", "bkspc");
  //FIXME: this will split line when cursor movement added
  cmd.line[curpos] = '\0';
  curpos--;
  mflag |= EX_LEFT;
  if (curpos < CURS_MIN)
    curpos = CURS_MIN;

  if (ex_state == EX_CMD_STATE) {
    if (curpos < cmd_stack[cur_part]->st)
      mflag |= EX_EMPTY;
  }
  mflag &= ~EX_FRESH;
}

static void ex_killword()
{
  Token *last = cmdline_last(&cmd);
  if (!last)
    return;

  int st = last->start;
  int ed = last->end;
  for (int i = st; i < ed; i++)
    cmd.line[i] = ' ';

  curpos = st;
  ex_bckspc();
}

static void ex_killline()
{
  free(cmd.line);
  cmd.line = calloc(maxpos, sizeof(char*));
  curpos = -1;

  ex_cmd_pop(-1);
  menu_restart(menu);
  mflag = 0;
}

static void ex_check_pipe()
{
  char pch = ex_cmd_prevstr()[0];
  char ch = ex_cmd_curstr()[0];
  log_msg("::::::::", "prev: %c cur: %c", pch, ch);
  if (pch == '<' && ch == '|')
    menu_restart(menu);
  else if (pch == '|' && ch == '>')
    menu_restart(menu);
  else if (ch == '|') {
    menu_restart(menu);
    mflag = EX_PUSH;
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
  if (curpos + 1 > tok->end && curpos > 0)
    mflag |= EX_NEW;
}

static void ex_onkey()
{
  log_msg("EXCMD", "##%d", ex_cmd_state());
  if (ex_state == EX_CMD_STATE) {
    cmdline_build(&cmd);
    check_new_state();
    menu_update(menu, &cmd);
  }
  else {
    if (window_focus_attached() && curpos > CURS_MIN) {
      regex_build(lm, cmd.line);
      regex_pivot(lm);
      regex_hover(lm);
    }
  }
  mflag &= ~EX_CLEAR;
  window_req_draw(NULL, NULL);
}

void ex_input(int key)
{
  log_msg("EXCMD", "input");
  Keyarg ca;
  int idx = find_command(&key_tbl, key);
  ca.arg = key_defaults[idx].cmd_arg;
  if (idx >= 0)
    key_defaults[idx].cmd_func(NULL, &ca);
  else {
    curpos++;
    mflag |= EX_RIGHT;
    mflag &= ~(EX_FRESH|EX_NEW);
    if (curpos >= maxpos)
      cmd.line = realloc(cmd.line, maxpos *= 2);

    // FIXME: wide char support
    cmd.line[curpos] = key;
  }

  if (mflag & EX_QUIT)
    stop_ex_cmd();
  else
    ex_onkey();
}

void ex_cmd_push(fn_context *cx)
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
  if (cur_part > 0)
    st = curpos;

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
  return cmd.line[curpos];
}

int ex_cmd_curpos()
{
  return curpos;
}

Token* ex_cmd_curtok()
{
  int st = cmd_stack[cur_part]->st;
  int ed = curpos + 1;
  Token *tok = cmdline_tokbtwn(&cmd, st, ed);
  return tok;
}

String ex_cmd_curstr()
{
  Token *tok = ex_cmd_curtok();
  if (tok)
    return token_val(tok, VAR_STRING);
  return "";
}

String ex_cmd_prevstr()
{
  if (cur_part < 1)
    return "";
  int st = cmd_stack[cur_part-1]->st;
  int ed = curpos;
  Token *tok = cmdline_tokbtwn(&cmd, st, ed);
  if (tok)
    return token_val(tok, VAR_STRING);
  return "";
}

int ex_cmd_state()
{
  return mflag;
}

void ex_cmd_set(int pos)
{
  curpos = pos;
}

List* ex_cmd_curlist()
{
  if (ex_state == EX_OFF_STATE)
    return NULL;
  if (!cmd.cmds)
    return NULL;

  Cmdstr *cmdstr = (Cmdstr*)utarray_next(cmd.cmds, ex_cmd_curtok());
  if (!cmdstr)
    cmdstr = (Cmdstr*)utarray_front(cmd.cmds);
  List *list = token_val(&cmdstr->args, VAR_LIST);
  return list;
}

int ex_cmd_curidx(List *list)
{
  return utarray_eltidx(list->items, ex_cmd_curtok());
}
