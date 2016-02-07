#ifndef FN_EVENT_HOOK_H
#define FN_EVENT_HOOK_H

#include "fnav/plugins/plugin.h"

typedef void (*hook_cb)(Plugin *host, Plugin *caller, void *data);

void send_hook_msg(String msg, Plugin *host, Plugin *caller, void *data);
void hook_init(Plugin *host);
void hook_cleanup(Plugin *host);

void hook_add(Plugin *host, Plugin *caller, hook_cb fn, String msg, int gbl);
void hook_remove(Plugin *host, Plugin *caller, String msg);

void hook_clear_msg(Plugin *host, String msg);
void hook_clear(Plugin *host);

#endif
