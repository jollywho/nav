#include <stdarg.h>
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

typedef struct compl_state compl_state;
struct compl_state {
  int argc;
  int st;
  compl_context *cx;   //context state
  compl_state   *prev; //previous state TODO: DLL when editable ex_mode
};

typedef struct {
  compl_state   *cs;
} Compl;

static Compl      cmpl;
static compl_list cmplist;

static compl_context *cxtbl;
static compl_context *cxroot;

static const UT_icd icd = {sizeof(compl_item),NULL,NULL,NULL};
static void mk_cmd_params(compl_context*cx, const char *, const char *);
compl_genfn find_gen(const char *);

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
  {"autocmd",   "-window:-group:event:pat:cmd", "_win:_aug:_event::"},
  {"augroup",   "group",                        "_aug"},
  {"bdelete",   "window",                       "_win"},
  {"buffer",    "-window:plugin",               "_win:_plug"},
  {"cd",        "path",                         "_path"},
  {"close",     "window",                       "_win"},
  {"delmark",   "label",                        "_mrklbl"},
  {"direct",    "window",                       "_win"},
  {"highlight", "group",                        "_group"},
  {"kill",      "pid",                          "_pid"},
  {"mark",      "label",                        "_mrklbl"},
  {"new",       "plugin",                       "_plug"},
  {"op",        "group",                        "_group"},
  {"set",       "option",                       "_option"},
  {"sort",      "type",                         "_field"},
  {"vnew",      "plugin",                       "_plug"},
};

static char *cmd_args[][3] = {
  {"plugin", "fm",  "path:paths"},
  {"plugin", "img", "window:wins"},
  {"plugin", "dt",  "path:paths"},
};

void compl_init()
{
  cmpl.cs = NULL;
  cxroot = malloc(sizeof(compl_context));
  cxroot->key = "cmd";
  mk_cmd_params(cxroot, "cmd", "_cmd");
  utarray_new(cmplist.rows,    &icd);
  utarray_new(cmplist.matches, &icd);

  for (int i = 0; i < LENGTH(cmd_defs); i++) {
    compl_context*cx = malloc(sizeof(compl_context));
    cx->key = strdup(cmd_defs[i][0]);

    mk_cmd_params(cx, cmd_defs[i][1], cmd_defs[i][2]);
    HASH_ADD_STR(cxtbl, key, cx);
  }
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
    cx->params[i]->flag = *p == '-' ? *p++ : 0;
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
  cmplist.comp_type = 0;
  cmplist.invalid_pos = 0;
}

void compl_list_add(char *fmt, ...)
{
  compl_item ci;
  va_list args;
  va_start(args, fmt);
  vasprintf(&ci.key, fmt, args);
  va_end(args);
  ci.colcount = 0;
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

bool compl_isdynamic()
{
  if (compl_dead())
    return false;
  return cmplist.comp_type == COMPL_DYNAMIC;
}


static void compl_push(compl_context *cx, int argc)
{
  log_msg("COMPL", "compl_push");
  compl_state *cs = calloc(1, sizeof(compl_state));
  cs->cx = cx;
  cs->argc = argc;
  if (cmpl.cs) {
    cs->prev = cmpl.cs;
  }
  compl_clear();
  cmpl.cs = cs;
}

static void compl_pop()
{
  log_msg("COMPL", "compl_pop");
  if (!cmpl.cs)
    return;
  if (cmpl.cs->prev) {
    compl_state *cs = cmpl.cs;
    cmpl.cs = cmpl.cs->prev;
    free(cs);
    return;
  }
  free(cmpl.cs);
  cmpl.cs = NULL;
}

void compl_backward()
{
  log_msg("COMPL", "compl_drop");
  compl_pop();
}

bool compl_forward(char *key)
{
  log_msg("COMPL", "compl_search");
  if (!cmpl.cs->cx || !key) {
    log_msg("COMPL", "not available.");
    return false;
  }

  compl_context *find;
  HASH_FIND_STR(cxtbl, key, find);
  if (!find)
    return false;

  //search name in matches to get group

  //push new context or arg#

  int argc = cmpl.cs->argc;
  compl_context *cx = cmpl.cs->cx;
  if (argc < cx->argc)
    compl_push(NULL, argc+1);

  return true;
}

//call cs param and generate list
void compl_build(List *args)
{
  log_msg("COMPL", "compl_build");
  if (compl_dead())
    return;

  compl_state *cs = cmpl.cs;
  compl_context *cx = cs->cx;
  for (int i = cs->argc; i < cx->argc; i++) {
    log_err("COMPL", "--");
    cx->params[i]->gen(args);
    if (cx->params[i]->flag != '-')
      break;
  }
}

int cmp_match(const void *a, const void *b, void *arg)
{
  compl_item c1 = *(compl_item*)a;
  compl_item c2 = *(compl_item*)b;

  int n1 = fuzzystrspn(c1.key, arg);
  int n2 = fuzzystrspn(c2.key, arg);
  return n2 - n1;
}

void compl_update(char *line)
{
  log_msg("COMPL", "compl_update");

  if (!line || compl_dead() || cmplist.invalid_pos)
    return;

  line = strip_shell(line);

  utarray_clear(cmplist.matches);
  cmplist.matchcount = 0;

  for (int i = 0; i < utarray_len(cmplist.rows); i++) {
    compl_item *ci = (compl_item*)utarray_eltptr(cmplist.rows, i);
    if (fuzzy_match(ci->key, line)) {
      utarray_push_back(cmplist.matches, ci);
      cmplist.matchcount++;
    }
  }

  utarray_sort(cmplist.matches, cmp_match, line);
  free(line);
}

compl_item* compl_idx_match(int idx)
{
  return (compl_item*)utarray_eltptr(cmplist.matches, idx);
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

int compl_last_pos()
{
  if (!cmpl.cs->cx || !cmpl.cs->prev)
    return -1;
  return cmpl.cs->prev->st;
}

int compl_cur_pos()
{
  return cmpl.cs->st;
}

bool compl_dead()
{
  return !cmpl.cs->cx;
}

void compl_begin()
{
  compl_push(cxroot, 0);
}

void compl_end()
{
  while (cmpl.cs)
    compl_pop();
}
