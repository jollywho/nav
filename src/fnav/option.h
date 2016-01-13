#ifndef FN_OPTION_H
#define FN_OPTION_H

#include "fnav/fnav.h"
#include "fnav/lib/uthash.h"

extern char *p_sh;          /* 'shell' */
extern char *p_cp;          /* 'copy   cmd' */
extern char *p_mv;          /* 'move   cmd' */
extern char *p_rm;          /* 'remove cmd' */

typedef struct {
  UT_hash_handle hh;
  int pair;
  int fg;
  int bg;
  char *key;
} fn_color;

void option_init();
void option_cleanup();
void set_color(fn_color *color);
int attr_color(const String name);

#endif
