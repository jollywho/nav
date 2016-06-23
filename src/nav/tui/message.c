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
#include "nav/util.h"

int dialog_pending;
int message_pending;
static int gch;
static WINDOW *win;

static void message_draw(char *line, int color)
{
  draw_wide(win, 0, 0, line, layout_size().col);
  mvwchgat(win, 0, 0, strlen(line), A_NORMAL, color, NULL);
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

  int color = opt_color(MSG_ASK);

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

static void msg(int color, char *line)
{
  message_start();
  message_pending = 1;
  message_draw(line, color);
  free(line);
}

void nv_err(char *fmt, ...)
{
  int color = opt_color(MSG_ERROR);
  char *line;
  va_list args;
  va_start(args, fmt);
  vasprintf(&line, fmt, args);
  va_end(args);
  msg(color, line);
}

void nv_msg(char *fmt, ...)
{
  char *line;
  va_list args;
  va_start(args, fmt);
  vasprintf(&line, fmt, args);
  va_end(args);
  msg(0, line);
}

void msg_clear()
{
  message_pending = 0;
  message_stop();
}

void dialog_input(int key)
{
  gch = key;
}
