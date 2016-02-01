#ifndef FN_PLUGINS_TERM_H
#define FN_PLUGINS_TERM_H

#include <uv.h>
#include "fnav/lib/sys_queue.h"
#include "fnav/plugins/plugin.h"
#include "fnav/vt/vt.h"

typedef struct term Term;

struct term {
  Plugin *base;
  Vt *vt;
  uv_poll_t readfd;
  Buffer *buf;
  SLIST_ENTRY(term) ent;
  int pid;
  int status;
  bool closed;
};

void term_new(Plugin *plugin, Buffer *buf, void *arg);
void term_delete(Plugin *plugin);
void term_keypress(Plugin *plugin, int key);
void term_cursor(Plugin *plugin);

#endif
