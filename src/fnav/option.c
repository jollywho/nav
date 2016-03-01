#include <curses.h>

#include "fnav/log.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/vt/vt.h"
#include "fnav/event/input.h"

static fn_color  *hi_colors;
static fn_var    *gbl_vars;
static fn_func   *gbl_funcs;

static struct fn_option {
  char *key;
  int flags;
  void *value;
} options[] = {
  {"hintkeys",  VAR_STRING,  "waesg"},
};

#define FLUSH_OLD_OPT(type,opt,str,expr)       \
  do {                                         \
    type *find;                                \
    HASH_FIND_STR(opt, (str), (find));         \
    if (find) {                                \
      HASH_DEL(opt, find);                     \
      free(find->key);                         \
      expr;                                    \
      free(find);                              \
    }                                          \
  }                                            \
  while (0)                                    \

#define CLEAR_OPT(type,opt,expr)               \
  do {                                         \
    type *it, *tmp;                            \
    HASH_ITER(hh, opt, it, tmp) {              \
      HASH_DEL(opt, it);                       \
      free(it->key);                           \
      expr;                                    \
      free(it);                                \
    }                                          \
  } while(0)                                   \

void option_init()
{
  init_pair(0, 0, 0);
}

void option_cleanup()
{
  CLEAR_OPT(fn_color, hi_colors, {});
  CLEAR_OPT(fn_var,   gbl_vars,  {});
  CLEAR_OPT(fn_func,  gbl_funcs, utarray_free(it->lines));
}

void set_color(fn_color *color)
{
  log_msg("OPTION", "set_color");
  fn_color *col = malloc(sizeof(fn_color));
  memmove(col, color, sizeof(fn_color));

  FLUSH_OLD_OPT(fn_color, hi_colors, col->key, {});

  col->pair = vt_color_get(NULL, col->fg, col->bg);
  color_set(col->pair, NULL);
  HASH_ADD_STR(hi_colors, key, col);
}

int attr_color(const char *name)
{
  fn_color *color;
  HASH_FIND_STR(hi_colors, name, color);
  if (!color)
    return 0;
  return color->pair;
}

void set_var(fn_var *variable)
{
  fn_var *var = malloc(sizeof(fn_var));
  memmove(var, variable, sizeof(fn_var));

  log_msg("CONFIG", "%s := %s", var->key, var->var);
  FLUSH_OLD_OPT(fn_var, gbl_vars, var->key, free(find->var));
  HASH_ADD_STR(gbl_vars, key, var);
}

char* opt_var(const char *name)
{
  fn_var *var;
  HASH_FIND_STR(gbl_vars, name, var);
  if (!var)
    return 0;
  return var->var;
}

void set_func(fn_func *func)
{
  fn_func *fn = malloc(sizeof(fn_func));
  memmove(fn, func, sizeof(fn_func));
  fn->key = strdup(func->key);

  log_msg("CONFIG", "%s :: %s", fn->key, *(char**)utarray_front(fn->lines));
  FLUSH_OLD_OPT(fn_func, gbl_funcs, fn->key, utarray_free(find->lines));
  HASH_ADD_STR(gbl_funcs, key, fn);
}

fn_func* opt_func(const char *name)
{
  fn_func *fn;
  HASH_FIND_STR(gbl_funcs, name, fn);
  return fn;
}

void* get_opt(const char *name)
{
  //FIXME: for test only
  return (void*)options[0].value;
}
