#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fnav/log.h"

#define _C  "\033[33m"
#define _R  "\033[0m"

#define LOG_FILENAME ".fnavlog"
FILE *log_file;

const char* listen_obj;

void log_init()
{
  char *name = getenv("HOME");
  fprintf(stdout, "%s\n", name);
  char *path;
  asprintf(&path, "%s/%s", name, LOG_FILENAME);
  log_file = fopen(path, "a");
}

void log_set(const char* obj)
{
  listen_obj = obj;
}

void log_msg(const char* obj, const char* fmt, ...)
{
  if (listen_obj && strcmp(listen_obj, obj) != 0)
    return;

  fprintf(log_file, "[" _C "%s" _R "] ", obj);
  va_list args;
  va_start(args, fmt);
  vfprintf(log_file, fmt, args);
  va_end(args);
  fputc('\n', log_file);
}
