#ifndef FN_EVENT_HOOK_H
#define FN_EVENT_HOOK_H

#include "fnav/plugins/plugin.h"

typedef struct Hook Hook;
typedef void (*hook_cb)(Plugin *host, Plugin *caller, void *data);

struct Hook {
  hook_cb fn;
  Plugin *caller;
  Plugin *host;
  String msg;
};

void send_hook_msg(String msg, Plugin *host, Plugin *caller, void *data);
void hook_init(Plugin *host);
void hook_cleanup(Plugin *host);

void hook_add(Plugin *host, Plugin *caller, hook_cb fn, String msg);
void hook_remove(Plugin *host, Plugin *caller, String msg);
void hook_clear_msg(Plugin *host, String msg);
void hook_clear(Plugin *host);

#endif
