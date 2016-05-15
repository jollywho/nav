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
#define HK_TMP  3 /* for OP plugin until extension */

typedef struct {
  char type;       //type of hook: HK_{INTL,CMD,TMP}
  char *msg;       //hooked event msg. TODO: replace with handler ptr
  char *grp;       //augroup name
  Plugin *caller;
  Plugin *host;
  Pattern *pat;
  union {
    hook_cb fn;
    char *cmd;
  } data;
} Hook;

typedef struct {
  char *key;
  UT_hash_handle hh;
} Augroup;

typedef struct {
  char *msg;
  UT_hash_handle hh;
  UT_array *hooks;   /* Hook */
} EventHandler;

struct HookHandler {
  UT_array *hosting; /* Hook */
  UT_array *own;     /* Hook */
};

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

void hook_init_host(Plugin *host)
{
  log_msg("HOOK", "INIT");
  host->event_hooks = calloc(1, sizeof(HookHandler));
  utarray_new(host->event_hooks->hosting, &hook_icd);
  utarray_new(host->event_hooks->own, &hook_icd);
}

void hook_cleanup_host(Plugin *host)
{
  log_msg("HOOK", "CLEANUP");
  utarray_free(host->event_hooks->hosting);
  utarray_free(host->event_hooks->own);
  free(host->event_hooks);
}

void hook_group_add(char *key)
{
  log_msg("HOOK", "GROUP ADD");
  Augroup *aug = malloc(sizeof(Augroup));
  aug->key = strdup(key);

  hook_group_remove(key);
  HASH_ADD_STR(aug_tbl, key, aug);
}

void hook_group_remove(char *key)
{
  log_msg("HOOK", "GROUP REMOVE");
  Augroup *find;
  HASH_FIND_STR(aug_tbl, key, find);
  if (find) {
    HASH_DEL(aug_tbl, find);
    free(find->key);
    free(find);
  }
}

void hook_add(char *event, char *pattern, char *expr)
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

  Hook hook = { HK_CMD, evh->msg, NULL, NULL, NULL, pat, .data.cmd = expr };
  utarray_push_back(evh->hooks, &hook);
}

void hook_remove(char *event, char *pattern)
{
  log_msg("HOOK", "REMOVE");
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, event, evh);
  if (!evh)
    return;
  Hook *it = NULL;
  for (int i = 0; i < utarray_len(evh->hooks); i++) {
    it = (Hook*)utarray_eltptr(evh->hooks, i);
    if (it->type == HK_CMD) {
      free(it->data.cmd);
      utarray_erase(evh->hooks, i, 1);
    }
  }
}

void hook_add_intl(Plugin *host, Plugin *caller, hook_cb fn, char *msg)
{
  log_msg("HOOK", "ADD_INTL");
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  if (!evh)
    return;

  Hook hook = { HK_INTL, evh->msg, NULL, caller, host, NULL, .data.fn = fn };
  utarray_push_back(evh->hooks, &hook);

  HookHandler *hkh = host->event_hooks;
  HookHandler *ckh = host->event_hooks;
  if (caller)
    ckh = caller->event_hooks;
  utarray_push_back(hkh->hosting, &hook);
  utarray_push_back(ckh->own, &hook);
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
  HookHandler *hkh = host->event_hooks;
  HookHandler *ckh = host->event_hooks;
  if (caller)
    ckh = caller->event_hooks;
  hook_rm_container(hkh->hosting, host, caller);
  hook_rm_container(ckh->own, host, caller);
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

void hook_clear_host(Plugin *host)
{
  log_msg("HOOK", "clear");
  HookHandler *hkh = host->event_hooks;

  //TODO: workaround to store actual pointers to hook in events_tbl
  for (int i = 0; i < utarray_len(hkh->own); i++) {
    Hook *it = (Hook*)utarray_eltptr(hkh->own, i);
    EventHandler *evh;
    HASH_FIND_STR(events_tbl, it->msg, evh);
    hook_rm_container(evh->hooks, it->host, it->caller);
    HookHandler *hkh = it->host->event_hooks;
    hook_rm_container(hkh->hosting, it->host, it->caller);
    HookHandler *ckh = it->host->event_hooks;
    if (it->caller)
      ckh = it->caller->event_hooks;
    hook_rm_container(ckh->hosting, it->host, it->caller);
  }
  utarray_clear(hkh->own);
  utarray_clear(hkh->hosting);
}

EventHandler* find_evh(char *msg)
{
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  return evh;
}

char* isgroup(char *key)
{
  if (!key)
    return NULL;

  Augroup *find;
  HASH_FIND_STR(aug_tbl, key, find);
  if (find)
    return key;

  return NULL;
}

char* isevent(char *key)
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
