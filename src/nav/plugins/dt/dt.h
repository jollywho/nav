#ifndef FN_PLUGINS_DT_H
#define FN_PLUGINS_DT_H

#include "nav/plugins/plugin.h"

typedef struct DT DT;

struct DT {
  Plugin *base;
  FILE *f;
  char *filename;
  Model *m;
};

void dt_new(Plugin *plugin, Buffer *buf, void *arg);
void dt_delete(Plugin *plugin);

#endif
