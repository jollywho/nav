#include <stdarg.h>

#include "nav/lib/uthash.h"
#include "nav/lib/utarray.h"
#include "nav/log.h"
#include "nav/event/hook.h"
#include "nav/cmdline.h"
#include "nav/compl.h"
#include "nav/regex.h"
#include "nav/util.h"

#define HK_INTL 1
#define HK_CMD  2
#define HK_TMP  3 /* TODO: use bufno + internal evh */

typedef struct {
  char *key;
  UT_hash_handle hh;
} Augroup;

typedef struct {
  int bufno;
  char type;       //type of hook: HK_{INTL,CMD,TMP}
  Augroup *aug;
  Plugin *caller;
  Plugin *host;
  Pattern *pat;
  union {
    hook_cb fn;
    char *cmd;
  } data;
} Hook;

typedef struct {
  char *msg;
  UT_hash_handle hh;
  UT_array *hooks;   /* Hook */
} EventHandler;

static EventHandler default_events[] = {
  [EVENT_CURSOR_CHANGE]  = { "cursorchange" },
  [EVENT_DIROPEN]        = { "diropen"      },
  [EVENT_EXEC_CLOSE]     = { "execclose"    },
  [EVENT_EXEC_OPEN]      = { "execopen"     },
  [EVENT_FILEOPEN]       = { "fileopen"     },
  [EVENT_JUMP]           = { "jump"         },
  [EVENT_LEFT]           = { "left"         },
  [EVENT_OPEN]           = { "open"         },
  [EVENT_OPTION_SET]     = { "optionset"    },
  [EVENT_PASTE]          = { "paste"        },
  [EVENT_PIPE]           = { "pipe"         },
  [EVENT_PIPE_REMOVE]    = { "piperemove"   },
  [EVENT_REMOVE]         = { "remove"       },
  [EVENT_RIGHT]          = { "right"        },
  [EVENT_WINDOW_RESIZE]  = { "windowresize" },
};

static UT_icd hook_icd = { sizeof(Hook),NULL,NULL,NULL };
static Augroup *aug_tbl;

void hook_init()
{
  for (int i = 0; i < LENGTH(default_events); i++) {
    EventHandler *evh = &default_events[i];
    utarray_new(evh->hooks, &hook_icd);
  }
}

void hook_cleanup()
{
  //TODO: hook_remove each entry in events_tbl
}

static EventHandler* get_event(enum nv_event ev)
{
  return ev < LENGTH(default_events) ? &default_events[ev] : NULL;
}

int event_idx(char *key)
{
  for (int i = 0; i < LENGTH(default_events); i++) {
    if (!strcmp(default_events[i].msg, key))
      return i;
  }
  return LENGTH(default_events)+1;
}

void augroup_add(char *key)
{
  log_msg("HOOK", "GROUP ADD");
  Augroup *aug = malloc(sizeof(Augroup));
  aug->key = strdup(key);

  augroup_remove(key);
  HASH_ADD_STR(aug_tbl, key, aug);
}

void augroup_remove(char *key)
{
  log_msg("HOOK", "GROUP REMOVE");
  Augroup *find;
  HASH_FIND_STR(aug_tbl, key, find);
  if (!find)
    return;

  HASH_DEL(aug_tbl, find);
  free(find->key);
  free(find);
}

static void augroup_insert(char *group, Hook *hk)
{
  log_msg("HOOK", "AUGROUP INSERT");
  if (!group)
    return;

  Augroup *find;
  HASH_FIND_STR(aug_tbl, group, find);
  if (!find)
    return;

  hk->aug = find;
}

void hook_add(char *group, char *event, char *pattern, char *expr, int id)
{
  log_msg("HOOK", "ADD");
  log_msg("HOOK", "<%s> %s `%s`", event, pattern, expr);

  EventHandler *evh = get_event(event_idx(event));
  if (!evh)
    return;

  expr = strip_quotes(expr);

  Pattern *pat = NULL;
  if (pattern)
    pat = regex_pat_new(pattern);

  Hook hook = { id, HK_CMD, NULL, NULL, NULL, pat, .data.cmd = expr };
  augroup_insert(group, &hook);
  utarray_push_back(evh->hooks, &hook);
}

static void hook_delete(EventHandler *evh, Augroup *aug)
{
  for (int i = 0; i < utarray_len(evh->hooks); i++) {
    Hook *it = (Hook*)utarray_eltptr(evh->hooks, i);
    if (it->type == HK_CMD && it->aug == aug) {
      free(it->data.cmd);
      utarray_erase(evh->hooks, i, 1);
    }
  }
}

void hook_remove(char *group, char *event, char *pattern)
{
  log_msg("HOOK", "REMOVE");
  log_msg("HOOK", "<%s> %s `%s`", event, pattern, group);

  Augroup *aug = NULL;
  if (group)
    HASH_FIND_STR(aug_tbl, group, aug);

  if (event) {
    EventHandler *evh = get_event(event_idx(event));
    if (!evh)
      return;
    return hook_delete(evh, aug);
  }

  EventHandler *it;
  for (it = default_events; it != NULL; it = it->hh.next)
    hook_delete(it, aug);
}

void hook_clear_host(int id)
{
  log_msg("HOOK", "CLEAR HOST");
  EventHandler *evh;
  for (evh = default_events; evh != NULL; evh = evh->hh.next) {
    for (int i = 0; i < utarray_len(evh->hooks); i++) {
      Hook *it = (Hook*)utarray_eltptr(evh->hooks, i);
      if (it->bufno == id) {
        if (it->type == HK_CMD)
          free(it->data.cmd);
        utarray_erase(evh->hooks, i, 1);
      }
    }
  }
}

void hook_add_intl(int id, Plugin *host, Plugin *caller, hook_cb fn, int ev)
{
  log_msg("HOOK", "ADD_INTL");
  EventHandler *evh = get_event(ev);

  Hook hook = { id, HK_INTL, NULL, caller, host, NULL, .data.fn = fn };
  utarray_push_back(evh->hooks, &hook);
}

static void hook_rm_container(UT_array *ary, Plugin *host, Plugin *caller)
{
  for (int i = 0; i < utarray_len(ary); i++) {
    Hook *it = (Hook*)utarray_eltptr(ary, i);
    if (it->host == host && it->caller == caller) {
      log_err("HOOK", "RM_INTL");
      utarray_erase(ary, i, 1);
    }
  }
}

void hook_rm_intl(Plugin *host, Plugin *caller, hook_cb fn, int ev)
{
  log_msg("HOOK", "RM_INTL");
  EventHandler *evh = get_event(ev);
  if (!evh)
    return;
  hook_rm_container(evh->hooks, host, caller);
}

void hook_set_tmp(int id)
{
  EventHandler *evh = get_event(id);
  if (!evh)
    return;
  Hook *hk = (Hook*)utarray_back(evh->hooks);
  hk->type = HK_TMP;
}

char* hook_group_name(char *key)
{
  if (!key)
    return NULL;

  Augroup *find;
  HASH_FIND_STR(aug_tbl, key, find);
  if (find)
    return key;

  return NULL;
}

char* hook_event_name(char *key)
{
  if (!key)
    return NULL;
  EventHandler *evh = get_event(event_idx(key));
  if (evh)
    return evh->msg;
  return NULL;
}

void call_intl_hook(Hook *hook, Plugin *host, Plugin *caller, void *data)
{
  log_msg("HOOK", "call_intl_hook");
  if (caller)
    hook->data.fn(host, caller, data);
  else
    hook->data.fn(host, hook->caller, data);
}

void call_cmd_hook(Hook *hook, HookArg *hka)
{
  log_msg("HOOK", "call_cmd_hook");
  if (hook->pat && hka && !regex_match(hook->pat, hka->arg))
    return;
  Cmdline cmd;
  cmdline_build(&cmd, hook->data.cmd);
  cmdline_req_run(NULL, &cmd);
  cmdline_cleanup(&cmd);
}

void call_hooks(EventHandler *evh, Plugin *host, Plugin *caller, HookArg *hka)
{
  if (!evh)
    return;

  Hook *it = NULL;
  while ((it = (Hook*)utarray_next(evh->hooks, it))) {

    if (it->type == HK_CMD) {
      int id = id_from_plugin(host);
      if (it->bufno == -1 || (host && id == it->bufno))
        call_cmd_hook(it, hka);
      continue;
    }
    if (it->host != host && it->type != HK_TMP)
      continue;

    call_intl_hook(it, host, caller, hka);
  }
}

void send_hook_msg(int ev, Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("HOOK", "(<%d>) msg sent", ev);
  call_hooks(get_event(ev), host, caller, hka);
}

void event_list()
{
  for (int i = 0; i < LENGTH(default_events); i++)
    compl_list_add("%s", default_events[i]);
}

void augs_list()
{
  int i = 0;
  Augroup *it;
  for (it = aug_tbl; it != NULL; it = it->hh.next) {
    compl_list_add("%s", it->key);
    //compl_set_col(i, "%s", #GROUP_COUNT#);
    i++;
  }
}
