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

static const char *events_list[] = {
  "open","fileopen","cursor_change","window_resize",
  "diropen","pipe_left","pipe_right","left","right","paste",
  "remove","execopen","execclose","pipe_remove","jump",
};

static UT_icd hook_icd = { sizeof(Hook),NULL,NULL,NULL };
static EventHandler *events_tbl;
static Augroup *aug_tbl;

void hook_init()
{
  for (int i = 0; i < LENGTH(events_list); i++) {
    EventHandler *evh = malloc(sizeof(EventHandler));
    evh->msg = strdup(events_list[i]);
    utarray_new(evh->hooks, &hook_icd);
    HASH_ADD_STR(events_tbl, msg, evh);
  }
}

void hook_cleanup()
{
  //TODO: hook_remove each entry in events_tbl
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

  EventHandler *evh;
  HASH_FIND_STR(events_tbl, event, evh);
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
      free(it);
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
    EventHandler *evh;
    HASH_FIND_STR(events_tbl, event, evh);
    if (!evh)
      return;
    return hook_delete(evh, aug);
  }

  EventHandler *it;
  for (it = events_tbl; it != NULL; it = it->hh.next)
    hook_delete(it, aug);
}

void hook_clear_host(int id)
{
  log_msg("HOOK", "CLEAR HOST");
  EventHandler *evh;
  for (evh = events_tbl; evh != NULL; evh = evh->hh.next) {
    for (int i = 0; i < utarray_len(evh->hooks); i++) {
      Hook *it = (Hook*)utarray_eltptr(evh->hooks, i);
      if (it->bufno == id) {
        if (it->type == HK_CMD)
          free(it->data.cmd);
        free(it);
        utarray_erase(evh->hooks, i, 1);
      }
    }
  }
}

void hook_add_intl(int id, Plugin *host, Plugin *caller, hook_cb fn, char *msg)
{
  log_msg("HOOK", "ADD_INTL");
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  if (!evh)
    return;

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

void hook_rm_intl(Plugin *host, Plugin *caller, hook_cb fn, char *msg)
{
  log_msg("HOOK", "RM_INTL");
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  if (!evh)
    return;
  hook_rm_container(evh->hooks, host, caller);
}

void hook_set_tmp(char *msg)
{
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  if (!evh) {
    log_err("HOOK", "not a supported event: %s", msg);
    return;
  }
  Hook *hk = (Hook*)utarray_back(evh->hooks);
  hk->type = HK_TMP;
}

EventHandler* find_evh(char *msg)
{
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  return evh;
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

  if (find_evh(key))
    return key;

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
      if (it->bufno == -1 || (host && host->id == it->bufno))
        call_cmd_hook(it, hka);
      continue;
    }
    if (it->host != host && it->type != HK_TMP)
      continue;

    call_intl_hook(it, host, caller, hka);
  }
}

void send_hook_msg(char *msg, Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("HOOK", "(<%s>) msg sent", msg);
  call_hooks(find_evh(msg), host, caller, hka);

  if (!host)
    return;
}

void event_list(List *args)
{
  compl_new(LENGTH(events_list), COMPL_STATIC);
  for (int i = 0; i < LENGTH(events_list); i++) {
    compl_set_key(i, "%s", events_list[i]);
  }
}

void augs_list(List *args)
{
  int i = 0;
  Augroup *it;
  compl_new(HASH_COUNT(aug_tbl), COMPL_STATIC);
  for (it = aug_tbl; it != NULL; it = it->hh.next) {
    compl_set_key(i, "%s", it->key);
    //compl_set_col(i, "%s", #GROUP_COUNT#);
    i++;
  }
}
