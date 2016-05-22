#include <curses.h>

#include "nav/log.h"
#include "nav/option.h"
#include "nav/cmdline.h"
#include "nav/cmd.h"
#include "nav/vt/vt.h"
#include "nav/event/input.h"
#include "nav/compl.h"
#include "nav/util.h"
#include "nav/expand.h"

enum opt_type { OPTION_STRING, OPTION_INT, OPTION_UINT };

static const char *default_groups[] = {
  [BUF_SEL_ACTIVE]       = "BufSelActive",
  [BUF_SEL_INACTIVE]     = "BufSelInactive",
  [COMPL_SELECTED]       = "ComplSelected",
  [BUF_TEXT]             = "BufText",
  [BUF_DIR]              = "BufDir",
  [BUF_SZ]               = "BufSz",
  [OVERLAY_SEP]          = "OverlaySep",
  [OVERLAY_LINE]         = "OverlayLine",
  [OVERLAY_BUFNO]        = "OverlayBufNo",
  [OVERLAY_ACTIVE]       = "OverlayActive",
  [OVERLAY_ARGS]         = "OverlayArgs",
  [OVERLAY_INACTIVE]     = "OverlayInactive",
  [OVERLAY_TEXTINACTIVE] = "OverlayTextInactive",
  [OVERLAY_PROGRESS]     = "OverlayProgress",
  [OVERLAY_FILTER]       = "OverlayFilter",
  [COMPL_TEXT]           = "ComplText",
  [COMPL_PARAM]          = "ComplParam",
  [MSG_ERROR]            = "MsgError",
  [MSG_MESSAGE]          = "MsgMessage",
  [MSG_ASK]              = "MsgAsk",
};

static int dummy = 0;
static uint history = 50;
static uint jumplist = 20;
static int menu_rows = 5;
static int default_syn_color;
static int ask_delete = 1;
static char *hintskey = "wasgd";
static char *p_sh = "/bin/sh";
char *p_rm = "rm -r";
char *p_xc = "xclip -i";
char *sep_chr = "│";

typedef struct fn_option fn_option;
static struct fn_option {
  char *key;
  enum opt_type type;
  void *value;
  UT_hash_handle hh;
} default_options[] = {
  {"dummy",         OPTION_INT,       &dummy},
  {"ask_delete",    OPTION_INT,       &ask_delete},
  {"history",       OPTION_UINT,      &history},
  {"jumplist",      OPTION_UINT,      &jumplist},
  {"menu_rows",     OPTION_INT,       &menu_rows},
  {"hintkeys",      OPTION_STRING,    &hintskey},
  {"shell",         OPTION_STRING,    &p_sh},
  {"sepchar",       OPTION_STRING,    &sep_chr},
  {"copy-pipe",     OPTION_STRING,    &p_xc},
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

static fn_group  *groups;
static fn_syn    *syntaxes;
static fn_var    *gbl_vars;
static fn_func   *gbl_funcs;
static fn_option *options;

const char *builtin_color(enum nv_color_group col) {
  return col < LENGTH(default_groups) ? default_groups[col] : NULL;
}

void option_init()
{
  init_pair(0, 0, 0);
  for (int i = 0; i < LENGTH(default_options); i++) {
    fn_option *opt = &default_options[i];
    if (default_options[i].type == OPTION_STRING)
      opt->value = strdup(*(char**)default_options[i].value);
    HASH_ADD_STR(options, key, opt);
  }
  for (int i = 0; i < LENGTH(default_groups); i++) {
    set_group(default_groups[i]);
  }
  default_syn_color = vt_color_get(NULL, 231, -1);
}

void option_cleanup()
{
  CLEAR_OPT(fn_syn,   syntaxes,  {});
  CLEAR_OPT(fn_var,   gbl_vars,  free(it->var));
  CLEAR_OPT(fn_group, groups,    op_delgrp(it->opgrp));
  CLEAR_OPT(fn_func,  gbl_funcs, {
    del_param_list(it->argv, it->argc);
    utarray_free(it->lines);
    clear_locals(it);
  });

  fn_option *it, *tmp;
  HASH_ITER(hh, options, it, tmp) {
    HASH_DEL(options, it);
    if (it->type == OPTION_STRING)
      free(it->value);
  }
}

void clear_locals(fn_func *func)
{
  CLEAR_OPT(fn_var, func->locals, free(it->var));
  func->locals = NULL;
}

void set_color(fn_group *grp, int fg, int bg)
{
  log_msg("OPTION", "set_color");
  grp->colorpair = vt_color_get(NULL, fg, bg);
}

short opt_color(enum nv_color_group color)
{
  const char *key = builtin_color(color);
  fn_group *grp;
  HASH_FIND_STR(groups, key, grp);
  if (!grp)
    return 0;
  return grp->colorpair;
}

fn_group* set_group(const char *name)
{
  fn_group *syg = malloc(sizeof(fn_group));
  memset(syg, 0, sizeof(fn_group));
  syg->key = strdup(name);
  syg->colorpair = 0;

  FLUSH_OLD_OPT(fn_group, groups, syg->key, {});
  HASH_ADD_STR(groups, key, syg);
  return syg;
}

fn_group* get_group(const char *name)
{
  fn_group *grp;
  HASH_FIND_STR(groups, name, grp);
  if (!grp)
    return 0;
  return grp;
}

void set_syn(fn_syn *syn)
{
  fn_syn *sy = malloc(sizeof(fn_syn));
  memmove(sy, syn, sizeof(fn_syn));
  FLUSH_OLD_OPT(fn_syn, syntaxes, sy->key, {});
  HASH_ADD_STR(syntaxes, key, sy);
}

fn_syn* get_syn(const char *name)
{
  fn_syn *sy;
  HASH_FIND_STR(syntaxes, name, sy);
  if (!sy)
    return 0;
  return sy;
}

int get_syn_colpair(const char *name)
{
  fn_syn *sy = get_syn(name);
  if (!sy)
    return default_syn_color;
  return sy->group->colorpair;
}

void set_var(fn_var *variable, fn_func *blk)
{
  fn_var *var = malloc(sizeof(fn_var));
  memmove(var, variable, sizeof(fn_var));

  log_msg("CONFIG", "%s := %s", var->key, var->var);
  fn_var **container = &gbl_vars;
  if (blk)
    container = &blk->locals;
  FLUSH_OLD_OPT(fn_var, *container, var->key, free(find->var));
  HASH_ADD_STR(*container, key, var);
}

char* opt_var(Token *word, fn_func *blk)
{
  log_msg("OPT", "opt_var");
  char *key = token_val(word, VAR_STRING);
  char *alt = NULL;
  if (!key) {
    Pair *p = token_val(word, VAR_PAIR);
    key = token_val(&p->key, VAR_STRING);
    alt = token_val(&p->value, VAR_STRING);
  }

  if (!key || !key[0])
    return strdup("");
  if (*key == '%')
    return expand_symbol(key+1, alt);
  if (*key == '$')
    key++;

  fn_var *var = NULL;

  if (blk)
    HASH_FIND_STR(blk->locals, key, var);
  if (!var)
    HASH_FIND_STR(gbl_vars, key, var);
  if (!var) {
    char *env = getenv(key);
    if (env)
      return env;
    else
      return strdup("");
  }

  return strdup(var->var);
}

void set_func(fn_func *func)
{
  fn_func *fn = func;
  fn->locals = NULL;
  HASH_ADD_STR(gbl_funcs, key, fn);
}

fn_func* opt_func(const char *name)
{
  fn_func *fn;
  HASH_FIND_STR(gbl_funcs, name, fn);
  return fn;
}

void set_opt(const char *name, const char *val)
{
  fn_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (!opt)
    return;

  log_msg("OPTION", "%s :: %s", opt->key, val);
  if (opt->type == OPTION_STRING)
    SWAP_ALLOC_PTR(opt->value, strdup(val));
  else if (opt->type == OPTION_INT) {
    int v_int;
    if (!str_num(val, &v_int))
      return;
    *(int*)opt->value = v_int;
  }
  else if (opt->type == OPTION_UINT) {
    uint v_uint;
    if (!str_tfmt(val, "%d", &v_uint))
      return;
    *(uint*)opt->value = v_uint;
  }
}

char* get_opt_str(const char *name)
{
  fn_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (opt->type == OPTION_STRING)
    return opt->value;
  else
    return NULL;
}

uint get_opt_uint(const char *name)
{
  fn_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (opt->type == OPTION_UINT)
    return *(uint*)opt->value;
  else
    return 0;
}

int get_opt_int(const char *name)
{
  fn_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (opt->type == OPTION_INT)
    return *(int*)opt->value;
  else
    return 0;
}

void options_list(List *args)
{
  log_msg("INFO", "setting_list");
  fn_option *it;
  int i = 0;
  for (it = options; it != NULL; it = it->hh.next) {
    compl_list_add("%s", it->key);
    if (it->type == OPTION_STRING)
      compl_set_col(i, "%s", (char*)it->value);
    else if (it->type == OPTION_INT || it->type == OPTION_UINT)
      compl_set_col(i, "%d", *(int*)it->value);
    i++;
  }
}

void groups_list(List *args)
{
  log_msg("INFO", "setting_list");
  fn_group *it;
  int i = 0;
  for (it = groups; it != NULL; it = it->hh.next) {
    compl_list_add("%s", it->key);
    i++;
  }
}
