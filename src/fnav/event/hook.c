#include <stdarg.h>

#include "fnav/lib/utarray.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"

struct HookHandler {
  UT_array *hooks;
};

static UT_icd hook_icd = { sizeof(Hook), NULL };

void hook_init(Cntlr *host)
{
  log_msg("HOOK", "INIT");
  host->event_hooks = malloc(sizeof(HookHandler));
  utarray_new(host->event_hooks->hooks, &hook_icd);
}

int hook_cmp(const void *a, const void *b)
{
  Hook *c1 = (Hook*)a;
  Hook *c2 = (Hook*)b;
  return strcmp(c1->msg, c2->msg);
}

void hook_add(Cntlr *host, Cntlr *caller, hook_cb fn, String msg)
{
  log_msg("HOOK", "ADD");
  Hook hook = { fn, caller, msg };
  utarray_push_back(host->event_hooks->hooks, &hook);
  utarray_sort(host->event_hooks->hooks, hook_cmp);
}

void send_hook_msg(String msg, Cntlr *host, Cntlr *caller)
{
  Hook *hook = (Hook*)utarray_find(host->event_hooks->hooks, msg, hook_cmp);
  if (!caller)
    caller = hook->caller;
}
