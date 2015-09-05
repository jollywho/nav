#define _C  "\033[33m"
#define _R  "\033[0m"

#include <stdarg.h>
#include <stdio.h>

#include "fnav/log.h"

void log_msg(const char* obj, const char* fmt, ...)
{
  fprintf(stderr, "[" _C "%s" _R "] ", obj);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}
