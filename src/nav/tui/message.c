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
int message_pending;
static int gch;
static WINDOW *win;

static void message_draw(char *line, int color)
{
  mvwprintw(win, 0, 0, line);
  mvwchgat(win, 0, 0, -1, A_NORMAL, color, NULL);
  wnoutrefresh(win);
  doupdate();
}

static void message_start()
{
  pos_T max = layout_size();
  win = newwin(1, 0, max.lnum - 1, 0);
}

static void message_stop()
{
  gch = NUL;
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

  message_start();
  dialog_pending = 1;

  message_draw(msg, color);
  while (gch == NUL)
    event_cycle_once();
  free(msg);

  int ret = 0;
  if (gch == 'Y' || gch == 'y' || gch == CAR)
    ret = 1;

  dialog_pending = 0;
  message_stop();
  return ret;
}

void nv_err(char *fmt, ...)
{
  char *msg;
  va_list args;
  va_start(args, fmt);
  vasprintf(&msg, fmt, args);
  va_end(args);

  int color = attr_color("MsgError");

  message_start();
  message_pending = 1;
  message_draw(msg, color);
  free(msg);
}

void nv_clr_msg()
{
  message_pending = 0;
  message_stop();
}

void dialog_input(int key)
{
  gch = key;
}
