#ifndef NV_PLUGINS_OUT_H
#define NV_PLUGINS_OUT_H

#include "nav/plugins/plugin.h"

typedef struct Out Out;

struct Out {
  Plugin *base;
  Handle *hndl;
  bool opened;
};

void out_init();
void out_new(Plugin *plugin, Buffer *buf, char *arg);
void out_delete(Plugin *plugin);

void out_recv(int, int, char *);

#endif
