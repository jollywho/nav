#ifndef FN_PLUGINS_TERM_H
#define FN_PLUGINS_TERM_H

#include <uv.h>
#include "fnav/plugins/plugin.h"
#include "fnav/vt/vt.h"

typedef struct Term Term;

struct Term {
  Plugin *base;
  Vt *vt;
  uv_poll_t readfd;
  WINDOW *win;
};

void term_new(Plugin *plugin, Buffer *buf);
void term_delete(Plugin *plugin);
void term_keypress(Plugin *plugin, int key);

#endif
