#ifndef NV_PLUGINS_TERM_H
#define NV_PLUGINS_TERM_H

#include <uv.h>
#include "nav/lib/sys_queue.h"
#include "nav/plugins/plugin.h"
#include "nav/plugins/ed/ed.h"
#include "nav/vt/vt.h"

typedef struct term Term;

struct term {
  Plugin *base;
  Vt *vt;
  uv_poll_t readfd;
  Buffer *buf;
  SLIST_ENTRY(term) ent;
  Ed *ed;
  int pid;
  int status;
  bool closed;
};

void term_new(Plugin *plugin, Buffer *buf, char *arg);
void term_delete(Plugin *plugin);
void term_keypress(Plugin *plugin, Keyarg *ca);
void term_cursor(Plugin *plugin);
void term_set_editor(Plugin *plugin, Ed *ed);
void term_close(Term *term);

#endif
