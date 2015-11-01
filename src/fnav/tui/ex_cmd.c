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

void cmdline_init()
{
  log_msg("CMDLINE", "init");
  pos_T max = layout_size();
  nc_win = newwin(1, 0, max.lnum - 1, 0);
  curstr = malloc(max.col * sizeof(char*));
  curpos = -1;
  maxpos = max.col;
  memset(curstr, '\0', maxpos);
}

void cmdline_draw()
{
  log_msg("CMDLINE", "curstr %s", curstr);
  mvwprintw(nc_win, 0, 0, ":");
  mvwprintw(nc_win, 0, 1, curstr);
  wmove(nc_win, 0, curpos + 2);
  wnoutrefresh(nc_win);
  doupdate();
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
  cmdline_draw();
}

void window_ex_cmd()
{
  cmdline_init();
  log_msg("EXCMD", "window_ex_cmd");
  char *test = "yqs eii {name: something, age:>1995 multi:\"long enough\" ids: [\"1\", \"tes tes\", 99]} | op download {id:33}";
  tokenize_line(&test);
  curs_set(1);
  cmdline_draw();
}

void stop_ex_cmd()
{
  log_msg("EXCMD", "stop_ex_cmd");
  free(curstr);
  wclear(nc_win);
  wnoutrefresh(nc_win);
  doupdate();
  curs_set(0);
  window_ex_cmd_end();
}
