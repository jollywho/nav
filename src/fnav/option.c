#include <curses.h>

#include "fnav/log.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/vt/vt.h"

typedef struct {
  fn_color *hi_colors;
  fn_var   *gbl_vars;
  // maps
  // setting
} fn_options;

fn_options *options;

#define FLUSH_OLD_OPT(type,opt,str,expr)        \
  type *find;                                   \
  HASH_FIND_STR(options->opt, (str), (find));   \
  if (find) {                                   \
    HASH_DEL(options->opt, find);               \
    free(find->key);                            \
    (expr);                                     \
    free(find);                                 \
  }                                             \

#define CLEAR_OPT(type,opt)                     \
  do {                                          \
    type *it, *tmp;                             \
    HASH_ITER(hh, options->opt, it, tmp) {      \
      HASH_DEL(options->opt, it);               \
      free(it->key);                            \
      free(it);                                 \
    }                                           \
  } while(0)                                    \

void option_init()
{
  options = malloc(sizeof(fn_options));
  memset(options, 0, sizeof(fn_options));
  init_pair(0, 0, 0);
}

void option_cleanup()
{
  CLEAR_OPT(fn_color, hi_colors);
  CLEAR_OPT(fn_var,   gbl_vars);
  free(options);
}

void set_color(fn_color *color)
{
  log_msg("OPTION", "set_color");
  fn_color *col = malloc(sizeof(fn_color));
  memmove(col, color, sizeof(fn_color));

  FLUSH_OLD_OPT(fn_color, hi_colors, col->key, {})

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

void set_var(fn_var *variable)
{
  fn_var *var = malloc(sizeof(fn_var));
  memmove(var, variable, sizeof(fn_var));

  log_msg("CONFIG", "%s := %s", var->key, var->var);
  FLUSH_OLD_OPT(fn_var, gbl_vars, var->key, free(find->var))

  HASH_ADD_STR(options->gbl_vars, key, var);
}

char* opt_var(const char *name)
{
  fn_var *var;
  HASH_FIND_STR(options->gbl_vars, name, var);
  if (!var)
    return 0;
  return var->var;
}
