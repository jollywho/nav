#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "nav/log.h"

#define _C  "\033[33m"
#define _R  "\033[0m"
#define _E  "\033[31m"
#define LOG_FILENAME ".navlog"

static FILE *log_file;
static int enabled;
static char* listen_obj;

void log_init()
{
  enabled = 1;
  if (log_file)
    return;
  char *name = getenv("HOME");
  char *path;
  asprintf(&path, "%s/%s", name, LOG_FILENAME);
  log_file = fopen(path, "w");
  free(path);
}

void log_cleanup()
{
  if (listen_obj)
    free(listen_obj);
  log_file = NULL;
}

void log_set_file(const char *path)
{
  log_file = fopen(path, "w");
}

void log_set_group(const char *obj)
{
  listen_obj = strdup(obj);
}

void log_msg(const char *obj, const char *fmt, ...)
{
  if (!enabled) return;
  if (listen_obj && strcasecmp(listen_obj, obj) != 0)
    return;

  fprintf(log_file, "[" _C "%s" _R "] ", obj);
  va_list args;
  va_start(args, fmt);
  vfprintf(log_file, fmt, args);
  va_end(args);
  fputc('\n', log_file);
  fflush(log_file);
}

void log_err(const char *obj, const char *fmt, ...)
{
  if (!enabled) return;
  if (listen_obj && strcasecmp(listen_obj, obj) != 0)
    return;

  fprintf(log_file, "[" _E "%s" _R "] ", obj);
  va_list args;
  va_start(args, fmt);
  vfprintf(log_file, fmt, args);
  va_end(args);
  fputc('\n', log_file);
  fflush(log_file);
}
