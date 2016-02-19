#include <curses.h>

#include "fnav/log.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/vt/vt.h"

typedef struct {
  fn_color *hi_colors;
  // maps
  // variables
  // setting
} fn_options;

fn_options *options;

void clear_colors()
{
  fn_color *it, *tmp;
  HASH_ITER(hh, options->hi_colors, it, tmp) {
    HASH_DEL(options->hi_colors, it);
    free(it->key);
    free(it);
  }
}

void option_init()
{
  options = malloc(sizeof(fn_options));
  memset(options, 0, sizeof(fn_options));
  init_pair(0, 0, 0);
}

void option_cleanup()
{
  clear_colors();
  free(options);
}

void set_color(fn_color *color)
{
  log_msg("OPTION", "set_color");
  fn_color *col = malloc(sizeof(fn_color));
  memmove(col, color, sizeof(fn_color));

  fn_color *find;
  HASH_FIND_STR(options->hi_colors, col->key, find);
  if (find) {
    HASH_DEL(options->hi_colors, find);
    free(find->key);
    free(find);
  }

  col->pair = vt_color_get(NULL, col->fg, col->bg);
  color_set(col->pair, NULL);
  HASH_ADD_STR(options->hi_colors, key, col);
}

int attr_color(const char *name)
{
  fn_color *color;
  HASH_FIND_STR(options->hi_colors, name, color);
  if (!color)
    return 0;
  return color->pair;
}
