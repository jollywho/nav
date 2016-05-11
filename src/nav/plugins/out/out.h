#ifndef FN_PLUGINS_OUT_H
#define FN_PLUGINS_OUT_H

#include "nav/plugins/plugin.h"

typedef struct Out Out;

struct Out {
  Plugin *base;
};

void out_init();
void out_new(Plugin *plugin, Buffer *buf, char *arg);
void out_delete(Plugin *plugin);

#endif
