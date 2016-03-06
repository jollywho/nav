#ifndef FN_PLUGINS_OP_H
#define FN_PLUGINS_OP_H

#include "fnav/plugins/plugin.h"

typedef struct Op Op;

struct Op {
  Plugin *base;
  fn_handle *hndl;
  bool ready;
};

void op_new(Plugin *plugin, Buffer *buf, void *arg);
void op_delete(Plugin *plugin);

#endif
