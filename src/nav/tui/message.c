#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <ncurses.h>
#include "nav/tui/message.h"
#include "nav/tui/layout.h"
#include "nav/log.h"
#include "nav/event/input.h"
#include "nav/option.h"

static void message_draw(WINDOW *win, char *line, int color)
{
  mvwprintw(win, 0, 0, line);
  mvwchgat(win, 0, 0, -1, A_NORMAL, color, NULL);
  wnoutrefresh(win);
  doupdate();
}

static WINDOW* message_start()
{
  pos_T max = layout_size();
  return newwin(1, 0, max.lnum - 1, 0);
}

static void message_end(WINDOW *win)
{
  werase(win);
  wnoutrefresh(win);
  doupdate();
  delwin(win);
}

int confirm(char *fmt, ...)
{
  char *msg;
  va_list args;
  va_start(args, fmt);
  vasprintf(&msg, fmt, args);
  va_end(args);

  int color = attr_color("MsgAsk");

  WINDOW *win = message_start();

  message_draw(win, msg, color);
  int ch = input_waitkey();
  free(msg);

  int ret = 0;
  if (ch == 'Y' || ch == 'y' || ch == CAR)
    ret = 1;

  message_end(win);
  return ret;
}
