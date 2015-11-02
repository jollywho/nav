// ex_cmd
// command line
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/tui/layout.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/cmdstr.h"

WINDOW *nc_win;
char *curstr;
int curpos;
int maxpos;
Cmdstr cmdstr;

#define CURMIN -1

void cmdline_init()
{
  log_msg("CMDLINE", "init");
  pos_T max = layout_size();
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curstr = malloc(max.col * sizeof(char*));
  curpos = CURMIN;
  maxpos = max.col;
  memset(curstr, '\0', maxpos);
  cmdstr_init(&cmdstr);
}

void cmdline_draw()
{
  log_msg("CMDLINE", "draw %s", curstr);
  mvwprintw(nc_win, 0, 0, ":");
  mvwprintw(nc_win, 0, 1, curstr);
  wmove(nc_win, 0, curpos + 2);
  wnoutrefresh(nc_win);
  doupdate();
}

void reset_line()
{
  memset(curstr, '\0', maxpos);
  wclear(nc_win);
  curpos = -1;
}

void del_word()
{
  int prev = cmdstr_prev_word(&cmdstr, curpos);

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
    // TODO: tokenize
    // TODO: rpc cmds table
    if (strcmp(curstr, "fm") == 0) {
      fm_init(bn->buf);
      window_ex_cmd_end();
    }
    if (strcmp(curstr, "sh") == 0) {
      sh_init(bn->buf);
      window_ex_cmd_end();
    }
    if (strcmp(curstr, "new") == 0) {
      pos_T dir = {.lnum = 1, .col = 0};
      window_add_buffer(dir);
    }
    if (strcmp(curstr, "vnew") == 0) {
      pos_T dir = {.lnum = 0, .col = 1};
      window_add_buffer(dir);
    }
    stop_ex_cmd();
    return;
  }
  if (key == BS) {
    curstr[curpos] = ' ';
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
      curstr = realloc(curstr, maxpos * sizeof(char*));
      curstr[maxpos] = '\0';
    }
    // FIXME: wide char support
    curstr[curpos] = key;
  }
  tokenize_line(&cmdstr, &curstr);
  cmdline_draw();
}

void window_ex_cmd()
{
  cmdline_init();
  log_msg("EXCMD", "window_ex_cmd");
  curs_set(1);
  cmdline_draw();
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  free(curstr);
  cmdstr_cleanup(&cmdstr);
  wclear(nc_win);
  wnoutrefresh(nc_win);
  doupdate();
  curs_set(0);
  window_ex_cmd_end();
}
