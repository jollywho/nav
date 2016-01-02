#include "fnav/tui/ex_cmd.h"
#include "fnav/fnav.h"
#include "fnav/cmdline.h"
#include "fnav/regex.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
#include "fnav/tui/menu.h"
#include "fnav/option.h"
#include "fnav/event/input.h"

#define STACK_MIN 10
static cmd_part **cmd_stack;
static int cur_part;
static int max_part;

static WINDOW *nc_win;
static int curpos;
static int maxpos;
static Cmdline cmd;
static String fmt_out;
static Menu *menu;

char state_symbol[] = {':', '/'};
static int ex_state;
static int col_text;
static int mflag;

#define CURS_MIN -1

static void cmdline_draw();
static void ex_esc();
static void ex_car();
static void ex_bckspc();
static void ex_killword();
static void ex_killline();

#define KEYS_SIZE ARRAY_SIZE(key_defaults)
static fn_key key_defaults[] = {
  {ESC,      ex_esc,           0,       0},
  {CAR,      ex_car,           0,       0},
  {BS,       ex_bckspc,        0,       0},
  {Ctrl_W,   ex_killword,      0,       0},
  {Ctrl_U,   ex_killline,      0,       0},
};
static fn_keytbl key_tbl;
static short cmd_idx[KEYS_SIZE];

void ex_cmd_init()
{
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = KEYS_SIZE;
  input_init_tbl(&key_tbl);
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
  if (window_get_focus() && state == EX_REG_STATE)
    regex_mk_pivot();
  else {
    cmd_stack = malloc(STACK_MIN * sizeof(cmd_part*));
    cur_part = -1;
    max_part = STACK_MIN;
    menu = menu_start();
  }
  cmdline_init(&cmd, max.col);
  cmdline_draw();
}

static void gen_output_str()
{
  char ch;
  int i, j;

  free(fmt_out);
  fmt_out = calloc(maxpos, maxpos * 2 * sizeof(char*));

  i = j = 0;
  while ((ch = cmd.line[i])) {
    fmt_out[j] = ch;

    if (ch == '%') {
      j++;
      fmt_out[j] = ch;
    }
    i++; j++;
  }
  fmt_out[maxpos*2] = '\0';
}

static void cmdline_draw()
{
  log_msg("EXCMD", "cmdline_draw");
  curs_set(1);

  if (menu)
    menu_draw(menu);

  wattron(nc_win, COLOR_PAIR(col_text));
  mvwaddch(nc_win, 0, 0, state_symbol[ex_state]);
  gen_output_str();
  mvwprintw(nc_win, 0, 1, fmt_out);
  wattroff(nc_win, COLOR_PAIR(col_text));

  wmove(nc_win, 0, curpos + 2);
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
  if (ex_state == 1) {
    regex_pivot();
  }
  mflag = EX_QUIT;
}

static void ex_car()
{
  if (ex_state == 0) {
    cmdline_req_run(&cmd);
  }
  else {
    regex_swap_pivot();
  }
  mflag = EX_QUIT;
}

static void ex_bckspc()
{
  werase(nc_win);
  //FIXME: this will split line when cursor movement added
  cmd.line[curpos] = '\0';
  curpos--;
  mflag |= EX_LEFT;
  if (curpos < CURS_MIN) {
    curpos = CURS_MIN;
  }
  if (curpos < cmd_stack[cur_part]->st) {
    mflag |= EX_EMPTY;
  }
  mflag &= ~EX_FRESH;
}

static void ex_killword()
{
  int st = cmdline_last(&cmd)->start;
  for (int i = st; i < curpos; i++)
    cmd.line[i] = '\0';

  curpos = st;
  ex_bckspc();
}

static void ex_killline()
{
  memset(cmd.line, '\0', maxpos);
  werase(nc_win);
  curpos = -1;

  ex_cmd_pop(cur_part);
  menu_restart(menu);
  mflag = 0;
}

static void ex_onkey()
{
  log_msg("MENU", "##%d", ex_cmd_state());
  if (ex_state == 0) {
    cmdline_build(&cmd);
    if ((mflag & EX_FRESH) != EX_FRESH) {
      Token *tok = cmdline_last(&cmd);
      if (tok) {
        if (curpos + 1 > tok->end && curpos > 0)
          mflag |= EX_NEW;
      }
    }
    menu_update(menu, &cmd);
  }
  else {
    if (window_focus_attached()) {
      regex_build(cmd.line);
      regex_pivot();
      regex_hover();
    }
  }
  mflag &= ~(EX_LEFT|EX_RIGHT|EX_EMPTY);
  cmdline_draw();
}

void ex_input(int key)
{
  log_msg("EXCMD", "input");

  Cmdarg ca;
  int idx = find_command(&key_tbl, key);
  ca.arg = key_defaults[idx].cmd_arg;
  if (idx >= 0) {
    key_defaults[idx].cmd_func(NULL, &ca);
  }
  else {
    curpos++;
    mflag |= EX_RIGHT;
    if (key != ' ') {
      mflag &= ~(EX_FRESH|EX_NEW);
    }
    if (curpos >= maxpos) {
      maxpos = 2 * curpos;
      cmd.line = realloc(cmd.line, maxpos * sizeof(char*));
      cmd.line[maxpos] = '\0';
    }
    // FIXME: wide char support
    cmd.line[curpos] = key;
  }
  if ((mflag & EX_QUIT) == EX_QUIT)
    stop_ex_cmd();
  else
    ex_onkey();
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  if (menu) {
    ex_cmd_pop(-1);
    free(cmd_stack);
    menu_stop(menu);
    menu = NULL;
  }
  free(cmd.line);
  free(fmt_out);
  cmdline_cleanup(&cmd);
  werase(nc_win);
  wnoutrefresh(nc_win);
  delwin(nc_win);
  curs_set(0);
  window_ex_cmd_end();
}

void ex_cmd_push(fn_context *cx)
{
  log_msg("EXCMD", "ex_cmd_push");
  cur_part++;

  if (cur_part >= max_part) {
    max_part = 2 * max_part;
    cmd_stack = realloc(cmd_stack, max_part * sizeof(cmd_part*));
  }

  cmd_stack[cur_part] = malloc(sizeof(cmd_part));
  cmd_stack[cur_part]->cx = cx;

  int st = 0;
  if (cur_part > 0) {
    st = curpos;
  }
  cmd_stack[cur_part]->st = st;
  mflag &= ~EX_NEW;
  mflag |= EX_FRESH;
}

cmd_part* ex_cmd_pop(int count)
{
  log_msg("EXCMD", "ex_cmd_pop");
  if (cur_part < 0) return NULL;

  if (count == -1)
    count = cur_part + 1;
  else
    count = MIN(cur_part, MAX(cur_part, cur_part - count));

  while (count > 0) {
    compl_destroy(cmd_stack[cur_part]->cx);
    free(cmd_stack[cur_part]);
    cur_part--;
    count--;
  }
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
    return TOKEN_STR(tok->var);
  else
    return "";
}

int ex_cmd_state()
{
  return mflag;
}
