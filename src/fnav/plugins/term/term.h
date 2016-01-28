#ifndef FN_PLUGINS_TERM_H
#define FN_PLUGINS_TERM_H

#include "fnav/plugins/plugin.h"
#include "fnav/event/pty_process.h"

typedef struct Term Term;

struct Term {
  Plugin *base;
  PtyProcess pty;
  Stream in;
  Stream out;
  Stream err;
};

void term_new(Plugin *plugin, Buffer *buf);
void term_delete(Plugin *plugin);
void term_write(Term *term, String msg);

#endif
