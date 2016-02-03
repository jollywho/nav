#include <stdarg.h>

#include "fnav/lib/utarray.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"

struct HookHandler {
  UT_array *hosted;
  UT_array *owner;
};

typedef struct HookList HookList;
struct HookList {
  UT_array *hooks;
  String msg;
};

static void hook_copy(void *_dst, const void *_src)
{
  Hook *dst = (Hook*)_dst, *src = (Hook*)_src;
  dst->caller = src->caller;
  dst->fn = src->fn;
  dst->msg = src->msg ? strdup(src->msg) : NULL;
}

static void hook_dtor(void *_elt)
{
  Hook *elt = (Hook*)_elt;
  if (elt->msg) free(elt->msg);
}

static UT_icd hook_icd = { sizeof(Hook),NULL,hook_copy,hook_dtor };
static UT_icd list_icd = { sizeof(HookList),NULL,NULL,NULL };

static int hook_cmp(const void *_a, const void *_b, void *arg)
{
  HookList *a = (HookList*)_a, *b = (HookList*)_b;
  return strcmp(a->msg, b->msg);
}

void hook_init(Plugin *host)
{
  log_msg("HOOK", "INIT");
  host->event_hooks = malloc(sizeof(HookHandler));
  utarray_new(host->event_hooks->hosted, &list_icd);
  utarray_new(host->event_hooks->owner, &hook_icd);
}

void hook_cleanup(Plugin *host)
{
  log_msg("HOOK", "CLEANUP");
  utarray_free(host->event_hooks->hosted);
  utarray_free(host->event_hooks->owner);
  free(host->event_hooks);
}

void hook_add(Plugin *host, Plugin *caller, hook_cb fn, String msg)
{
  log_msg("HOOK", "ADD");
  HookHandler *host_handle = host->event_hooks;
  HookList find = {.msg = msg, .hooks = NULL};

  HookList *hl = (HookList*)utarray_find(host_handle->hosted, &find, hook_cmp, 0);
  if (!hl) {
    //TODO add hook to caller's owner list
    Hook hook = {fn,caller,host, msg};
    utarray_new(find.hooks, &hook_icd);
    utarray_push_back(find.hooks, &hook);

    utarray_push_back(host_handle->hosted, &find);
    utarray_sort(host_handle->hosted, hook_cmp, 0);
  }
  else {
    Hook hook = {fn,caller,host, msg};
    utarray_push_back(hl->hooks, &hook);
  }
}

void hook_remove(Plugin *host, Plugin *caller, String msg)
{
  log_msg("HOOK", "remove");
  // TODO
  // find hook in caller's owner list by msg
  // remove hook from host's hosted list
  // remove the hosted list if empty
  // remove hook from owner's list
}

void hook_clear_msg(Plugin *host, String msg)
{
  // TODO
  // find hooklist in host by msg
  // iterate hooklist
  //  remove from hooklist
  //  remove from it->caller->owner
  // remove hooklist from host
}

void hook_clear(Plugin *host)
{
  log_msg("HOOK", "clear");
  // TODO
  // iterate host hooklist
  //  hook_clear_msg it
}

void send_hook_msg(String msg, Plugin *host, Plugin *caller, void *data)
{
  log_msg("HOOK", "(<%s>) msg sent", msg);
  if (!host)
    return;
  HookHandler *host_handle = host->event_hooks;
  HookList find = { .msg = msg };

  HookList *hl = (HookList*)utarray_find(host_handle->hosted, &find, hook_cmp, 0);
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
