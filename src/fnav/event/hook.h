#ifndef FN_EVENT_HOOK_H
#define FN_EVENT_HOOK_H

#include "fnav/plugins/plugin.h"

typedef void (*hook_cb)(Plugin *host, Plugin *caller, void *data);

void send_hook_msg(String msg, Plugin *host, Plugin *caller, void *data);
void hook_init();
void hook_cleanup();
void hook_init_host(Plugin *host);
void hook_cleanup_host(Plugin *host);

void hook_add();
void hook_remove();

void hook_add_intl(Plugin *host, Plugin *caller, hook_cb fn, String msg);
void hook_clear_msg(Plugin *host, String msg);
void hook_clear_host(Plugin *host);

void event_list(List *args);

#endif
