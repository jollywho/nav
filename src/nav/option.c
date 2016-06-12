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
  [BUF_STDOUT]           = "BufStdout",
  [BUF_STDERR]           = "BufStderr",
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

typedef struct nv_option nv_option;
static struct nv_option {
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

static nv_group  *groups;
static nv_syn    *syntaxes;
static nv_var    *gbl_vars;
static nv_func   *gbl_funcs;
static nv_option *options;

const char *builtin_color(enum nv_color_group col) {
  return col < LENGTH(default_groups) ? default_groups[col] : NULL;
}

void option_init()
{
  init_pair(0, 0, 0);
  for (int i = 0; i < LENGTH(default_options); i++) {
    nv_option *opt = &default_options[i];
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
  CLEAR_OPT(nv_syn,   syntaxes,  {});
  CLEAR_OPT(nv_var,   gbl_vars,  free(it->var));
  CLEAR_OPT(nv_group, groups,    op_delgrp(it->opgrp));
  CLEAR_OPT(nv_func,  gbl_funcs, {
    del_param_list(it->argv, it->argc);
    utarray_free(it->lines);
    clear_locals(it);
  });

  nv_option *it, *tmp;
  HASH_ITER(hh, options, it, tmp) {
    HASH_DEL(options, it);
    if (it->type == OPTION_STRING)
      free(it->value);
  }
}

void clear_locals(nv_func *func)
{
  CLEAR_OPT(nv_var, func->locals, free(it->var));
  func->locals = NULL;
}

void set_color(nv_group *grp, int fg, int bg)
{
  log_msg("OPTION", "set_color");
  grp->colorpair = vt_color_get(NULL, fg, bg);
}

short opt_color(enum nv_color_group color)
{
  const char *key = builtin_color(color);
  nv_group *grp;
  HASH_FIND_STR(groups, key, grp);
  if (!grp)
    return 0;
  return grp->colorpair;
}

nv_group* set_group(const char *name)
{
  nv_group *syg = malloc(sizeof(nv_group));
  memset(syg, 0, sizeof(nv_group));
  syg->key = strdup(name);
  syg->colorpair = 0;

  FLUSH_OLD_OPT(nv_group, groups, syg->key, {});
  HASH_ADD_STR(groups, key, syg);
  return syg;
}

nv_group* get_group(const char *name)
{
  nv_group *grp;
  HASH_FIND_STR(groups, name, grp);
  if (!grp)
    return 0;
  return grp;
}

void set_syn(nv_syn *syn)
{
  nv_syn *sy = malloc(sizeof(nv_syn));
  memmove(sy, syn, sizeof(nv_syn));
  FLUSH_OLD_OPT(nv_syn, syntaxes, sy->key, {});
  HASH_ADD_STR(syntaxes, key, sy);
}

nv_syn* get_syn(const char *name)
{
  nv_syn *sy;
  HASH_FIND_STR(syntaxes, name, sy);
  if (!sy)
    return 0;
  return sy;
}

int get_syn_colpair(const char *name)
{
  nv_syn *sy = get_syn(name);
  if (!sy)
    return default_syn_color;
  return sy->group->colorpair;
}

void set_var(nv_var *variable, nv_func *blk)
{
  nv_var *var = malloc(sizeof(nv_var));
  memmove(var, variable, sizeof(nv_var));

  log_msg("CONFIG", "%s := %s", var->key, var->var);
  nv_var **container = &gbl_vars;
  if (blk)
    container = &blk->locals;
  FLUSH_OLD_OPT(nv_var, *container, var->key, free(find->var));
  HASH_ADD_STR(*container, key, var);
}

char* opt_var(Token *word, nv_func *blk)
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

  nv_var *var = NULL;

  if (blk)
    HASH_FIND_STR(blk->locals, key, var);
  //TODO: lookup in source variables
  if (!var)
    HASH_FIND_STR(gbl_vars, key, var);
  if (!var) {
    char *env = getenv(key);
    if (env)
      return strdup(env);
    else
      return strdup("''");
  }

  return strdup(var->var);
}

void set_func(nv_func *func)
{
  nv_func *fn = func;
  fn->locals = NULL;
  HASH_ADD_STR(gbl_funcs, key, fn);
}

nv_func* opt_func(const char *name)
{
  nv_func *fn;
  HASH_FIND_STR(gbl_funcs, name, fn);
  return fn;
}

void set_opt(const char *name, const char *val)
{
  nv_option *opt;
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
  nv_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (opt->type == OPTION_STRING)
    return opt->value;
  else
    return NULL;
}

uint get_opt_uint(const char *name)
{
  nv_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (opt->type == OPTION_UINT)
    return *(uint*)opt->value;
  else
    return 0;
}

int get_opt_int(const char *name)
{
  nv_option *opt;
  HASH_FIND_STR(options, name, opt);
  if (opt->type == OPTION_INT)
    return *(int*)opt->value;
  else
    return 0;
}

void options_list(List *args)
{
  log_msg("INFO", "setting_list");
  nv_option *it;
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
  nv_group *it;
  int i = 0;
  for (it = groups; it != NULL; it = it->hh.next) {
    compl_list_add("%s", it->key);
    i++;
  }
}
