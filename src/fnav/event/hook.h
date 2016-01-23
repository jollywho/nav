#ifndef FN_EVENT_HOOK_H
#define FN_EVENT_HOOK_H

#include "fnav/tui/cntlr.h"

typedef struct Hook Hook;
typedef void (*hook_cb)(Cntlr *host, Cntlr *caller, void *data);

struct Hook {
  hook_cb fn;
  Cntlr *caller;
  Cntlr *host;
  String msg;
};

void send_hook_msg(String msg, Cntlr *host, Cntlr *caller, void *data);
void hook_init(Cntlr *host);
void hook_cleanup(Cntlr *host);

void hook_add(Cntlr *host, Cntlr *caller, hook_cb fn, String msg);
void hook_remove(Cntlr *host, Cntlr *caller, String msg);
void hook_clear_msg(Cntlr *host, String msg);
void hook_clear(Cntlr *host);

#endif
