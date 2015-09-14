#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "fnav/log.h"

#define _C  "\033[33m"
#define _R  "\033[0m"

const char* listen_obj;

void log_set(const char* obj)
{
  listen_obj = obj;
}

void log_msg(const char* obj, const char* fmt, ...)
{
#ifdef NCURSES_ENABLED
    return;
#endif
  if (listen_obj && strcmp(listen_obj, obj) != 0)
    return;
  fprintf(stderr, "[" _C "%s" _R "] ", obj);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}
