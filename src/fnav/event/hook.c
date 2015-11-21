#include <stdarg.h>

#include "fnav/lib/utarray.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"

struct HookHandler {
  UT_array *lists;
};
typedef struct HookList HookList;
struct HookList {
  UT_array *hooks;
  String msg;
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
static UT_icd list_icd =  { sizeof(HookList),NULL,NULL,NULL };

void hook_init(Cntlr *host)
{
  log_msg("HOOK", "INIT");
  host->event_hooks = malloc(sizeof(HookHandler));
  utarray_new(host->event_hooks->lists, &list_icd);
}

static int hook_cmp(const void *_a, const void *_b)
{
  HookList *a = (HookList*)_a, *b = (HookList*)_b;
  return strcmp(a->msg, b->msg);
}

void hook_add(Cntlr *host, Cntlr *caller, hook_cb fn, String msg)
{
  log_msg("HOOK", "ADD");
  HookList find = {.msg = msg};
  HookList *hl = (HookList*)utarray_find(host->event_hooks->lists,
      &find, hook_cmp);
  if (!hl) {
    utarray_new(find.hooks, &hook_icd);
    utarray_push_back(host->event_hooks->lists, &find);
    hl = &find;
  }
  Hook hook = {fn,caller,msg};
  utarray_push_back(hl->hooks, &hook);
  utarray_sort(hl->hooks, hook_cmp);
}

void send_hook_msg(String msg, Cntlr *host, Cntlr *caller)
{
  log_msg("HOOK", "(<%s>) msg sent", msg);
  HookList find = { .msg = msg };
  HookList *hl = (HookList*)utarray_find(host->event_hooks->lists,
      &find, hook_cmp);
  if (!hl) return;

  Hook *it = (Hook*)utarray_front(hl->hooks);
  if (caller) {
    it->fn(host, caller);
    return;
  }
  while (it) {
    it->fn(host, it->caller);
    it = (Hook*)utarray_next(hl->hooks, it);
  }
}
