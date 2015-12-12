#include <curses.h>

#include "fnav/log.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"

typedef struct {
  fn_color *hi_colors;
  // maps
  // variables
  // setting
  int max_col_pairs;
} fn_options;

fn_options *options;

#define HASH_ITEM(t,obj) \
  HASH_ADD_KEYPTR(hh, t, obj->key, strlen(obj->key), obj);

void option_init()
{
  options = malloc(sizeof(fn_options));
  options->max_col_pairs = 0;
}

void set_color(fn_color *color)
{
  log_msg("OPTION", "set_color");
  // check if exists
  color->pair = ++options->max_col_pairs;
  init_pair(color->pair, color->fg, color->bg);
  HASH_ITEM(options->hi_colors, color);
}

int attr_color(const String name)
{
  fn_color *color;
  HASH_FIND_STR(options->hi_colors, name, color);
  log_msg("OPTION", "set_color %d", color->pair);
  if (!color)
    return -1;
  return color->pair;
}
