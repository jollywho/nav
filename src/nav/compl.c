#include <stdarg.h>
#include "nav/lib/sys_queue.h"
#include "nav/compl.h"
#include "nav/plugins/plugin.h"
#include "nav/log.h"
#include "nav/cmd.h"
#include "nav/tui/menu.h"
#include "nav/table.h"
#include "nav/info.h"
#include "nav/event/hook.h"
#include "nav/option.h"
#include "nav/util.h"

static const UT_icd icd = {sizeof(compl_item),NULL,NULL,NULL};
static void mk_cmd_params(compl_context*cx, const char *, const char *);
compl_genfn find_gen(const char *);

typedef struct compl_state compl_state;
struct compl_state {
  int argc;
  int st;
  compl_context *cx;   //context state
  TAILQ_ENTRY(compl_state) ent;
};

typedef struct {
  TAILQ_HEAD(cont, compl_state) p;
  compl_state   *cs;     //current state
  compl_context *cxtbl;  //context table
  compl_context *cxroot; //context root entry
  bool rebuild;
  int build_arg;
  char rep;
  int block;
} Compl;

static Compl cmpl;
static compl_list cmplist;

//TODO:
//block: block level (quotes, arrays). ignore pushes if > 0

static struct compl_entry {
  char *key;
  compl_genfn gen;
} compl_defaults[] = {
  { "_cmd",      cmd_list     },
  { "_plug",     plugin_list  },
  { "_field",    field_list   },
  { "_win",      win_list     },
  { "_path",     path_list    },
  { "_mark",     mark_list    },
  { "_mrklbl",   marklbl_list },
  { "_event",    event_list   },
  { "_option",   options_list },
  { "_group",    groups_list  },
  { "_aug",      augs_list    },
  { "_pid",      pid_list     },
};

static char *cmd_defs[][3] = {
  {"!",         "*{CMD}",                       ""},
  {"autocmd",   "-WINDOW:-GROUP:EVENT:PAT:CMD", "_win:_aug:_event::"},
  {"augroup",   "GROUP",                        "_aug"},
  {"bdelete",   "WINDOW",                       "_win"},
  {"buffer",    "-WINDOW:PLUGIN",               "_win:_plug"},
  {"cd",        "PATH",                         "_path"},
  {"close",     "WINDOW",                       "_win"},
  {"delmark",   "LABEL",                        "_mrklbl"},
  {"direct",    "WINDOW",                       "_win"},
  {"echo",      "*{EXPR}",                      ""},
  {"highlight", "GROUP",                        "_group"},
  {"kill",      "PID",                          "_pid"},
  {"mark",      "LABEL",                        "_mrklbl"},
  {"new",       "PLUGIN",                       "_plug"},
  {"op",        "GROUP",                        "_group"},
  {"set",       "OPTION",                       "_option"},
  {"sort",      "TYPE",                         "_field"},
  {"vnew",      "PLUGIN",                       "_plug"},
};

static char *cmd_args[][3] = {
  {"plugin", "fm",  "PATH:paths"},
  {"plugin", "img", "WINDow:wins"},
  {"plugin", "dt",  "PATH:paths"},
};

void compl_init()
{
  cmpl.cs = NULL;
  cmpl.cxroot = malloc(sizeof(compl_context));
  cmpl.cxroot->key = "CMD";
  mk_cmd_params(cmpl.cxroot, "CMD", "_cmd");
  utarray_new(cmplist.rows,    &icd);
  utarray_new(cmplist.matches, &icd);

  for (int i = 0; i < LENGTH(cmd_defs); i++) {
    compl_context*cx = malloc(sizeof(compl_context));
    cx->key = strdup(cmd_defs[i][0]);

    mk_cmd_params(cx, cmd_defs[i][1], cmd_defs[i][2]);
    HASH_ADD_STR(cmpl.cxtbl, key, cx);
  }
  TAILQ_INIT(&cmpl.p);
}

void compl_cleanup()
{
}

static void mk_cmd_params(compl_context *cx, const char *tmpl, const char *str)
{
  cx->argc = 1 + count_strstr(str, ":");
  cx->params = malloc(cx->argc*sizeof(compl_param));

  char *t = strdup(tmpl);
  char *s = strdup(str);

  char *p = strtok(t, ":");
  for (int i = 0; i < cx->argc; i++) {
    cx->params[i] = malloc(sizeof(compl_param));
    switch (*p) {
      case '-':
      case '*':
        cx->params[i]->flag = *p++;
        break;
      default:
        cx->params[i]->flag = 0;
    }
    cx->params[i]->label = strdup(p);
    p = strtok(NULL, ":");
  }

  char *g = strtok(s, ":");
  for (int i = 0; i < cx->argc; i++) {
    cx->params[i]->gen = find_gen(g);
    g = strtok(NULL, ":");
  }

  free(t);
  free(s);
}

compl_genfn find_gen(const char *key)
{
  if (!key)
    return NULL;
  for (int i = 0; i < LENGTH(compl_defaults); i++) {
    if (!strcmp(key, compl_defaults[i].key))
      return compl_defaults[i].gen;
  }
  return NULL;
}

void compl_clear()
{
  log_msg("COMPL", "compl_clear");
  for (int i = 0; i < utarray_len(cmplist.rows); i++) {
    compl_item *ci = (compl_item*)utarray_eltptr(cmplist.rows, i);
    if (ci->colcount > 0)
      free(ci->columns);
    free(ci->key);
  }
  utarray_clear(cmplist.rows);
  utarray_clear(cmplist.matches);
  cmpl.rep = 0;
  cmplist.invalid_pos = 0;
}

void compl_list_add(const char *fmt, ...)
{
  compl_item ci;
  va_list args;
  va_start(args, fmt);
  vasprintf(&ci.key, fmt, args);
  va_end(args);
  ci.colcount = 0;
  ci.argc = cmpl.build_arg;
  utarray_push_back(cmplist.rows, &ci);
}

void compl_set_col(int idx, char *fmt, ...)
{
  compl_item *ci = (compl_item*)utarray_eltptr(cmplist.rows, idx);
  va_list args;
  va_start(args, fmt);
  vasprintf(&ci->columns, fmt, args);
  va_end(args);
  ci->colcount = 1;
}

void compl_set_repeat(char ch)
{
  cmpl.rep = ch;
}

void compl_next()
{
  if (TAILQ_PREV(cmpl.cs, cont, ent))
    cmpl.cs = TAILQ_PREV(cmpl.cs, cont, ent);
}

void compl_prev()
{
  if (TAILQ_NEXT(cmpl.cs, ent))
    cmpl.cs = TAILQ_NEXT(cmpl.cs, ent);
}

static void compl_push(compl_context *cx, int argc, int pos)
{
  log_msg("COMPL", "compl_push");
  compl_state *cs = calloc(1, sizeof(compl_state));
  cs->cx = cx;
  cs->argc = argc;
  cs->st = pos;
  TAILQ_INSERT_HEAD(&cmpl.p, cs, ent);
  cmpl.cs = cs;
  cmpl.rebuild = true;
}

static void compl_pop()
{
  log_msg("COMPL", "compl_pop");
  if (TAILQ_EMPTY(&cmpl.p))
    return;
  compl_state *cs = cmpl.cs;
  cmpl.cs = TAILQ_NEXT(cs, ent);
  TAILQ_REMOVE(&cmpl.p, cs, ent);
  free(cs);
  cmpl.rebuild = true;
}

void compl_backward()
{
  log_msg("COMPL", "compl_backward");
  compl_pop();
}

int cmp_match(const void *a, const void *b, void *arg)
{
  compl_item c1 = *(compl_item*)a;
  compl_item c2 = *(compl_item*)b;

  int n1 = fuzzystrspn(c1.key, arg);
  int n2 = fuzzystrspn(c2.key, arg);
  return n2 - n1;
}

void compl_filter(const char *src)
{
  log_msg("COMPL", "compl_filter");

  if (cmplist.invalid_pos)
    return;

  char *key = strip_shell(src);

  utarray_clear(cmplist.matches);
  cmplist.matchcount = 0;
  log_msg("COMPL", "[%s]", key);

  for (int i = 0; i < utarray_len(cmplist.rows); i++) {
    compl_item *ci = (compl_item*)utarray_eltptr(cmplist.rows, i);
    if (fuzzy_match(ci->key, key)) {
      utarray_push_back(cmplist.matches, ci);
      cmplist.matchcount++;
    }
  }

  utarray_sort(cmplist.matches, cmp_match, key);
  free(key);
}

static compl_context* compl_find(const char *key, const char *alt, int pos)
{
  compl_context *find;
  HASH_FIND_STR(cmpl.cxtbl, key, find);
  if (!find && alt)
    HASH_FIND_STR(cmpl.cxtbl, alt, find);
  if (find) {
    log_msg("COMPL", "push %s %d", find->key, pos);
    compl_push(find, 0, pos);
  }
  return find;
}

static void compl_search(compl_context *cx, const char *key, int pos)
{
  /* get next param */
  int argc = cmpl.cs->argc;
  for (int i = 0; i < utarray_len(cmplist.matches); i++) {
    compl_item *it = (compl_item*)utarray_eltptr(cmplist.matches, i);
    if (!strcmp(it->key, key)) {
      argc = it->argc;
      break;
    }
  }

  if (argc + 1 < cx->argc)
    return compl_push(cx, ++argc, pos);
  else if (cx->params[argc]->flag == '*')
    return;
  else
    cx = NULL;

  /* get next context */
  Cmd_T *cmd = cmd_find(key);
  if (cmd && (cx = compl_find(cmd->name, cmd->alt, pos)))
    return;

  /* push non-blank state */
  if (key[0] != ' ')
    compl_push(cx, cmpl.cs->argc, pos);
}

void compl_update(const char *key, int pos, char ch)
{
  log_msg("COMPL", "compl_update");
  log_msg("COMPL", "[%s] %c", key, ch);
  compl_context *cx = cmpl.cs->cx;
  if (!cx || !key || !key[0]) {
    log_msg("COMPL", "not available.");
    return;
  }

  if (ch == ' ')
    compl_search(cx, key, pos);
  else if (ch == cmpl.rep)
    compl_push(cx, cmpl.cs->argc, pos);
}

void compl_build(List *args)
{
  log_msg("COMPL", "compl_build");
  if (compl_dead() || !cmpl.rebuild)
    return;

  compl_clear();
  compl_state *cs = cmpl.cs;
  compl_context *cx = cs->cx;

  /* generate list from context params */
  for (int i = cs->argc; i < cx->argc; i++) {
    cmpl.build_arg = i;
    if (cx->params[i]->gen)
      cx->params[i]->gen(args);
    if (cx->params[i]->flag != '-')
      break;
  }
  cmpl.rebuild = false;
}

void compl_set_exec(int pos)
{
  log_msg("COMPL", "compl_set_exec");
  compl_find("!", NULL, pos);
}

compl_item* compl_idx_match(int idx)
{
  return (compl_item*)utarray_eltptr(cmplist.matches, idx);
}

void compl_walk_params(int (*param_cb)(char *,char,int,bool))
{
  compl_state *cs = cmpl.cs;
  compl_context *cx = cs->cx;

  int prev = 0;
  for (int i = 0; i < cx->argc; i++) {
    compl_param *param = cx->params[i];
    int sign = i == cs->argc;
    prev = param_cb(param->label, param->flag, prev, sign);
  }
}

compl_list* compl_complist()
{
  return &cmplist;
}

void compl_invalidate(int pos)
{
  utarray_clear(cmplist.matches);
  cmplist.matchcount = 0;
  cmplist.invalid_pos = pos;
}

bool compl_validate(int pos)
{
  return cmplist.invalid_pos > pos;
}

int compl_prev_pos()
{
  if (!cmpl.cs || TAILQ_PREV(cmpl.cs, cont, ent))
    return -1;
  return TAILQ_PREV(cmpl.cs, cont, ent)->st;
}

int compl_last_pos()
{
  if (!cmpl.cs || TAILQ_EMPTY(&cmpl.p) || !TAILQ_NEXT(cmpl.cs, ent))
    return -1;
  return TAILQ_NEXT(cmpl.cs, ent)->st;
}

int compl_cur_pos()
{
  if (!cmpl.cs)
    return -1;
  return cmpl.cs->st;
}

int compl_arg_pos()
{
  compl_state *cs = cmpl.cs;
  compl_context *cx = cs->cx;
  int argc = cs->argc;

  while (1) {
    compl_state *tmp = TAILQ_NEXT(cs, ent);
    if (!tmp || tmp->cx != cx || tmp->argc != argc)
      break;
    cs = TAILQ_NEXT(cs, ent);
  }

  return cs->st;
}

bool compl_dead()
{
  return !cmpl.cs || !cmpl.cs->cx;
}

void compl_begin(int pos)
{
  compl_push(cmpl.cxroot, 0, pos);
  compl_build(NULL);
  compl_filter("");
}

void compl_end()
{
  log_msg("MENU", "end");
  while (!TAILQ_EMPTY(&cmpl.p))
    compl_pop();
  compl_clear();
  free(cmpl.cs);
  cmpl.cs = NULL;
}
