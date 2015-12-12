#ifndef FN_OPTION_H
#define FN_OPTION_H

#include "fnav/fnav.h"
#include "fnav/lib/uthash.h"

typedef struct {
  UT_hash_handle hh;
  int pair;
  int fg;
  int bg;
  char *key;
} fn_color;

void option_init();
void set_color(fn_color *color);
int attr_color(const String name);

#endif
