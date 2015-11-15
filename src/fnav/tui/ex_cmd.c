// ex_cmd
// command line
#include "fnav/tui/ex_cmd.h"
#include "fnav/cmdline.h"
#include "fnav/regex.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"

WINDOW *nc_win;
int curpos;
int maxpos;
Cmdline cmd;

char state_symbol[] = {':', '/'};
int ex_state;

#define CURMIN -1

static void cmdline_draw();

void start_ex_cmd(int state)
{
  log_msg("EXCMD", "start");
  ex_state = state;
  pos_T max = layout_size();
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curpos = CURMIN;
  maxpos = max.col;
  if (window_get_focus() && state == EX_REG_STATE)
    regex_mk_pivot();
  cmdline_init(&cmd, max.col);
  cmdline_draw();
}

static void cmdline_draw()
{
  curs_set(1);
  mvwaddch(nc_win, 0, 0, state_symbol[ex_state]);
  mvwprintw(nc_win, 0, 1, cmd.line);
  wmove(nc_win, 0, curpos + 2);
  wnoutrefresh(nc_win);
  doupdate();
}

void cmdline_refresh()
{
  cmdline_draw();
}

void reset_line()
{
  memset(cmd.line, '\0', maxpos);
  wclear(nc_win);
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
    regex_keep_pivot();
  }
  stop_ex_cmd();
}

static void ex_onkey()
{
  if (ex_state == 0) {
    cmdline_build(&cmd);
  }
  else {
    regex_build(cmd.line);
    regex_pivot();
    regex_hover();
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
    wclear(nc_win);
    //FIXME: this will split line when cursor movement added
    cmd.line[curpos] = '\0';
    curpos--;
    if (curpos < CURMIN)
      curpos = CURMIN;
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
  cmdline_cleanup(&cmd);
  wclear(nc_win);
  wnoutrefresh(nc_win);
  doupdate();
  curs_set(0);
  window_ex_cmd_end();
}
