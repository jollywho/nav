#include <curses.h>

#include "fnav/log.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"

#define HASH_INS(t,obj) \
  HASH_ADD_KEYPTR(hh, t, obj->key, strlen(obj->key), obj);

typedef struct {
  fn_color *hi_colors;
  // maps
  // variables
  // setting
  int max_col_pairs;
} fn_options;

fn_options *options;

void option_init()
{
  options = malloc(sizeof(fn_options));
  options->max_col_pairs = 1;
  init_pair(0, 0, 0);
}

void set_color(fn_color *color)
{
  log_msg("OPTION", "set_color");

  fn_color *find;
  HASH_FIND_STR(options->hi_colors, color->key, find);
  if (find) {
    HASH_DEL(options->hi_colors, find);
    free(find);
  }

  color->pair = ++options->max_col_pairs;
  init_pair(color->pair, color->fg, color->bg);
  HASH_INS(options->hi_colors, color);
}

int attr_color(const String name)
{
  fn_color *color;
  HASH_FIND_STR(options->hi_colors, name, color);
  if (!color)
    return 0;
  return color->pair;
}
