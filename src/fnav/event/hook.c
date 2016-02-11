#include <stdarg.h>

#include "fnav/lib/uthash.h"
#include "fnav/lib/utarray.h"
#include "fnav/log.h"
#include "fnav/event/hook.h"
#include "fnav/cmdline.h"

#define HK_INTL 1
#define HK_CMD  2

typedef struct {
  char type;
  Plugin *caller;
  Plugin *host;
  union {
    hook_cb fn;
    String cmd;
  } data;
} Hook;

//TODO: group name for namespace
typedef struct {
  String msg;
  UT_hash_handle hh;
  UT_array *hooks;   /* Hook */
} EventHandler;

struct HookHandler {
  UT_array *hosting; /* Hook */
  UT_array *own;     /* Hook */
};

static const String event_list[] = {
  "open", "fileopen", "cursor_change", "window_resize", "diropen",
  "pipe_left", "pipe_right", "left", "right", "paste", "remove",
};

static UT_icd hook_icd = { sizeof(Hook),NULL,NULL,NULL };
static EventHandler *events_tbl;

void hook_init()
{
  for (int i = 0; i < LENGTH(event_list); i++) {
    EventHandler *evh = malloc(sizeof(EventHandler));
    evh->msg = strdup(event_list[i]);
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

void hook_add(String msg, String cmd)
{
}

void hook_remove(String msg, String cmd)
{
}

void hook_add_intl(Plugin *host, Plugin *caller, hook_cb fn, String msg)
{
  log_msg("HOOK", "ADD");
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  if (!evh)
    return;

  Hook hook = { HK_INTL, caller, host, .data.fn = fn };

  utarray_push_back(evh->hooks, &hook);

  HookHandler *hkh = host->event_hooks;
  utarray_push_back(hkh->hosting, &hook);
  utarray_push_back(hkh->own, &hook);
}

void hook_clear_host(Plugin *host)
{
  log_msg("HOOK", "clear");
  HookHandler *hkh = host->event_hooks;

  //TODO: workaround to store actual pointers to hook in events_tbl
  utarray_clear(hkh->own);
  utarray_clear(hkh->hosting);
}

EventHandler* find_evh(String msg)
{
  EventHandler *evh;
  HASH_FIND_STR(events_tbl, msg, evh);
  return evh;
}

void call_intl_hook(Hook *hook, Plugin *host, Plugin *caller, void *data)
{
  if (caller)
    hook->data.fn(host, caller, data);
  else
    hook->data.fn(host, hook->caller, data);
}

void call_cmd_hook(Hook *hook)
{
  Cmdline cmd;
  cmdline_init_config(&cmd, hook->data.cmd);
  cmdline_build(&cmd);
  cmdline_req_run(&cmd);
  cmdline_cleanup(&cmd);
}

void call_hooks(EventHandler *evh, Plugin *host, Plugin *caller, void *data)
{
  if (!evh)
    return;

  Hook *it = NULL;
  while ((it = (Hook*)utarray_next(evh->hooks, it))) {
    if (it->host != host)
      continue;

    if (it->type == HK_INTL)
      call_intl_hook(it, host, caller, data);
    else
      call_cmd_hook(it);
  }
}

void send_hook_msg(String msg, Plugin *host, Plugin *caller, void *data)
{
  log_msg("HOOK", "(<%s>) msg sent", msg);
  call_hooks(find_evh(msg), host, caller, data);

  if (!host)
    return;
}
