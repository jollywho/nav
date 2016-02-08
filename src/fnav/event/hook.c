#include <stdarg.h>

#include "fnav/lib/uthash.h"
#include "fnav/lib/utarray.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"

typedef struct {
  hook_cb fn;
  Plugin *caller;
  Plugin *host;
} Hook;

typedef struct {
  String msg;
  UT_hash_handle hh;
  UT_array *hooks;
} HookList;

struct HookHandler {
  HookList *hosted;
  UT_array *own;
};

static UT_icd hook_icd = { sizeof(Hook),NULL,NULL,NULL };
static HookList *global_hooks;

void hook_init(Plugin *host)
{
  log_msg("HOOK", "INIT");
  host->event_hooks = calloc(1, sizeof(HookHandler));
  utarray_new(host->event_hooks->own, &hook_icd);
}

void hook_cleanup(Plugin *host)
{
  log_msg("HOOK", "CLEANUP");
  utarray_free(host->event_hooks->own);
  free(host->event_hooks);
}

void hook_add(Plugin *host, Plugin *caller, hook_cb fn, String msg, int gbl)
{
  log_msg("HOOK", "ADD");
  HookHandler *host_handle = host->event_hooks;
  HookList **hl_cont = &host_handle->hosted;

  Hook hook = { fn, caller, host };
  if (gbl) {
    hook.host = NULL;
    hl_cont = &global_hooks;
  }

  HookList *find;
  HASH_FIND_STR(*hl_cont, msg, find);
  if (!find) {
    find = calloc(1, sizeof(HookList));
    find->msg = strdup(msg);
    utarray_new(find->hooks, &hook_icd);
    HASH_ADD_STR(*hl_cont, msg, find);
  }
  utarray_push_back(find->hooks, &hook);
  utarray_push_back(host_handle->own, &hook);
}

void hook_remove(Plugin *host, Plugin *caller, String msg)
{
  log_msg("HOOK", "remove");
  HookHandler *host_handle = host->event_hooks;
  HookList *find;
  HASH_FIND_STR(host_handle->hosted, msg, find);
  if (!find)
    return;

  Hook *it = (Hook*)utarray_front(find->hooks);
  for (int i = 0; i < utarray_len(find->hooks); i++) {
    if (it->caller == caller) {
      int idx = utarray_eltidx(find->hooks, it);
      utarray_erase(find->hooks, idx, 1);
      break;
    }
    it = (Hook*)utarray_next(find->hooks, it);
  }
}

//TODO: find+remove hooks from their own list
void hook_clear(Plugin *host)
{
  log_msg("HOOK", "clear");
  HookHandler *host_handle = host->event_hooks;
  HookList *hosted = host_handle->hosted;
  HookList *it, *tmp;
  HASH_ITER(hh, hosted, it, tmp) {
    utarray_clear(it->hooks);
    utarray_free(it->hooks);
    HASH_DEL(hosted, it);
    free(it->msg);
    free(it);
  }
}

HookList* find_hl(HookList *hl, String msg)
{
  HookList *find;
  HASH_FIND_STR(hl, msg, find);
  return find;
}

void call_hooks(HookList *hl, Plugin *host, Plugin *caller, void *data)
{
  if (!hl)
    return;

  Hook *it = (Hook*)utarray_front(hl->hooks);
  while (it) {
    if (caller)
      it->fn(host, caller, data);
    else
      it->fn(host, it->caller, data);
    it = (Hook*)utarray_next(hl->hooks, it);
  }
}

void send_hook_msg(String msg, Plugin *host, Plugin *caller, void *data)
{
  log_msg("HOOK", "(<%s>) msg sent", msg);
  call_hooks(find_hl(global_hooks, msg), host, caller, data);

  if (!host)
    return;
  HookHandler *host_handle = host->event_hooks;
  call_hooks(find_hl(host_handle->hosted, msg), host, caller, data);
}
