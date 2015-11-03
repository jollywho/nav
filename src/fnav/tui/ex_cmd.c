// ex_cmd
// command line
#include "fnav/tui/ex_cmd.h"
#include "fnav/cmdline.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"

WINDOW *nc_win;
int curpos;
int maxpos;
Cmdline cmd;

#define CURMIN -1

static void cmdline_draw();

void start_ex_cmd()
{
  log_msg("EXCMD", "start");
  pos_T max = layout_size();
  curs_set(1);
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curpos = CURMIN;
  maxpos = max.col;
  cmdline_init(&cmd, max.col);
  cmdline_draw();
}

static void cmdline_draw()
{
  log_msg("CMDLINE", "draw %s", cmd.line);
  mvwprintw(nc_win, 0, 0, ":");
  mvwprintw(nc_win, 0, 1, cmd.line);
  wmove(nc_win, 0, curpos + 2);
  wnoutrefresh(nc_win);
  doupdate();
}

void reset_line()
{
  memset(cmd.line, '\0', maxpos);
  wclear(nc_win);
  curpos = -1;
}

void del_word()
{
  int prev = cmdstr_prev_word(&cmd.line, curpos);
  // delete from curpos to found index
}

void ex_input(BufferNode *bn, int key)
{
  log_msg("EXCMD", "input");
  if (key == ESC) {
    stop_ex_cmd();
    return;
  }
  if (key == CAR) {
    // TODO: rpc cmds table
    if (strcmp(cmd.line, "fm") == 0) {
      fm_init(bn->buf);
      window_ex_cmd_end();
    }
    if (strcmp(cmd.line, "sh") == 0) {
      sh_init(bn->buf);
      window_ex_cmd_end();
    }
    if (strcmp(cmd.line, "new") == 0) {
      pos_T dir = {.lnum = 1, .col = 0};
      window_add_buffer(dir);
    }
    if (strcmp(cmd.line, "vnew") == 0) {
      pos_T dir = {.lnum = 0, .col = 1};
      window_add_buffer(dir);
    }
    stop_ex_cmd();
    return;
  }
  if (key == BS) {
    cmd.line[curpos] = ' ';
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
  cmdline_build(&cmd);
  cmdline_draw();
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  cmdline_destroy(&cmd);
  wclear(nc_win);
  wnoutrefresh(nc_win);
  doupdate();
  curs_set(0);
  window_ex_cmd_end();
}
