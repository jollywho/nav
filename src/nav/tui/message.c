#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <ncurses.h>
#include "nav/tui/message.h"
#include "nav/tui/layout.h"
#include "nav/log.h"
#include "nav/event/input.h"
#include "nav/event/event.h"
#include "nav/option.h"

int dialog_pending;
static int gch;

static void message_draw(WINDOW *win, char *line, int color)
{
  mvwprintw(win, 0, 0, line);
  mvwchgat(win, 0, 0, -1, A_NORMAL, color, NULL);
  wnoutrefresh(win);
  doupdate();
}

static WINDOW* message_start()
{
  dialog_pending = 1;
  pos_T max = layout_size();
  return newwin(1, 0, max.lnum - 1, 0);
}

static void message_stop(WINDOW *win)
{
  dialog_pending = 0;
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
  event_cycle_once();
  free(msg);

  int ret = 0;
  if (gch == 'Y' || gch == 'y' || gch == CAR)
    ret = 1;

  message_stop(win);
  return ret;
}

void dialog_input(int key)
{
  gch = key;
}
