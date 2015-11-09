#include <stdarg.h>

#include "fnav/lib/utarray.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"

struct HookHandler {
  UT_array *hooks;
};

void intchar_copy(void *_dst, const void *_src) {
  Hook *dst = (Hook*)_dst, *src = (Hook*)_src;
  dst->caller = src->caller;
  dst->fn = src->fn;
  dst->msg = src->msg ? strdup(src->msg) : NULL;
}

void intchar_dtor(void *_elt) {
  Hook *elt = (Hook*)_elt;
  if (elt->msg) free(elt->msg);
}
static UT_icd hook_icd = { sizeof(Hook),NULL,intchar_copy,intchar_dtor };

void hook_init(Cntlr *host)
{
  log_msg("HOOK", "INIT");
  host->event_hooks = malloc(sizeof(HookHandler));
  utarray_new(host->event_hooks->hooks, &hook_icd);
}

static int hook_cmp(const void *_a, const void *_b)
{
  Hook *a = (Hook*)_a, *b = (Hook*)_b;
  return strcmp(a->msg,b->msg);
}

void hook_add(Cntlr *host, Cntlr *caller, hook_cb fn, String msg)
{
  log_msg("HOOK", "ADD");
  Hook hook = {fn,caller,msg};
  utarray_push_back(host->event_hooks->hooks, &hook);
  utarray_sort(host->event_hooks->hooks, hook_cmp);
}

void send_hook_msg(String msg, Cntlr *host, Cntlr *caller)
{
  log_msg("HOOK", "(<%s>) msg sent", msg);
  Hook find = { .msg = msg };
  Hook *hook = (Hook*)utarray_find(host->event_hooks->hooks, &find, hook_cmp);
  if (!hook) return;
  if (!caller)
    caller = hook->caller;
  hook->fn(host, caller);
}
