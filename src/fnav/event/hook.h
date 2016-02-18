#ifndef FN_EVENT_HOOK_H
#define FN_EVENT_HOOK_H

#include "fnav/plugins/plugin.h"

typedef struct {
  Keyarg *ka;
  String arg;
} HookArg;

typedef void (*hook_cb)(Plugin *host, Plugin *caller, HookArg *hka);

void send_hook_msg(String msg, Plugin *host, Plugin *caller, HookArg *hka);
void hook_init();
void hook_cleanup();
void hook_init_host(Plugin *host);
void hook_cleanup_host(Plugin *host);

void hook_add(String event, String pattern, String cmd);
void hook_remove(String event, String pattern);

void hook_add_intl(Plugin *host, Plugin *caller, hook_cb fn, String msg);
void hook_set_tmp(String msg);
void hook_clear_msg(Plugin *host, String msg);
void hook_clear_host(Plugin *host);

void event_list(List *args);

#endif
