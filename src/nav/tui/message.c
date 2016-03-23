#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <ncurses.h>
#include "nav/tui/message.h"
#include "nav/log.h"
#include "nav/event/input.h"

int confirm(char *fmt, ...)
{
  char *msg;
  va_list args;
  va_start(args, fmt);
  vasprintf(&msg, fmt, args);
  va_end(args);

  log_err("-", "%s", msg);
  int ch = input_waitkey();
  free(msg);

  int ret = 0;
  if (ch == 'Y' || ch == 'y' || ch == CAR)
    ret = 1;

  return ret;
}
