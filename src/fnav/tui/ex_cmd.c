// ex_cmd
// command line
#include "fnav/tui/ex_cmd.h"
#include "fnav/cmdline.h"
#include "fnav/regex.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
#include "fnav/tui/menu.h"
#include "fnav/option.h"

static WINDOW *nc_win;
static int curpos;
static int maxpos;
static Cmdline cmd;
static String fmt_out;
static Menu *menu;

char state_symbol[] = {':', '/'};
static int ex_state;
static int col_text;

#define CURS_MIN -1

static void cmdline_draw();

void start_ex_cmd(int state)
{
  log_msg("EXCMD", "start");
  ex_state = state;
  pos_T max = layout_size();
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curpos = CURS_MIN;
  maxpos = max.col;
  fmt_out = malloc(1);
  col_text = attr_color("ComplText");
  if (window_get_focus() && state == EX_REG_STATE)
    regex_mk_pivot();
  else
    menu = menu_start();
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

void reset_line()
{
  memset(cmd.line, '\0', maxpos);
  werase(nc_win);
  curpos = -1;
}

void del_word()
{
  //int prev = cmdline_prev_word(&cmd, curpos);
  // delete from curpos to found index
}

static void ex_enter()
{
  if (ex_state == 0) {
    cmdline_req_run(&cmd);
  }
  else {
    regex_swap_pivot();
  }
  stop_ex_cmd();
}

static void ex_onkey()
{
  if (ex_state == 0) {
    cmdline_build(&cmd);
    menu_update(menu, &cmd);
  }
  else {
    if (window_focus_attached()) {
      regex_build(cmd.line);
      regex_pivot();
      regex_hover();
    }
  }
  cmdline_draw();
}

static void ex_onesc()
{
  if (ex_state == 1) {
    regex_pivot();
  }
  stop_ex_cmd();
}

void ex_input(int key)
{
  log_msg("EXCMD", "input");
  if (key == ESC) {
    ex_onesc();
    return;
  }
  if (key == CAR) {
    ex_enter();
    return;
  }
  if (key == BS) {
    werase(nc_win);
    //FIXME: this will split line when cursor movement added
    cmd.line[curpos] = '\0';
    curpos--;
    if (curpos < CURS_MIN)
      curpos = CURS_MIN;
  }
  else if (key == Ctrl_U) {
    reset_line();
  }
  else if (key == Ctrl_W) {
    del_word();
  }
  else {
    curpos++;
    if (curpos >= maxpos) {
      maxpos = 2 * curpos;
      cmd.line = realloc(cmd.line, maxpos * sizeof(char*));
      cmd.line[maxpos] = '\0';
    }
    // FIXME: wide char support
    cmd.line[curpos] = key;
  }
  ex_onkey();
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  if (menu) {
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
