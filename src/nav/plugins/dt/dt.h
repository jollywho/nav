#ifndef NV_PLUGINS_DT_H
#define NV_PLUGINS_DT_H

#include "nav/plugins/plugin.h"

typedef struct DT DT;

struct DT {
  Plugin *base;
  FILE *f;
  char *filename;
  Model *m;
};

void dt_new(Plugin *plugin, Buffer *buf, char *arg);
void dt_delete(Plugin *plugin);

#endif
