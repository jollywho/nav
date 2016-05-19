#ifndef FN_PLUGINS_OP_H
#define FN_PLUGINS_OP_H

#include "nav/lib/utarray.h"
#include "nav/plugins/plugin.h"

typedef struct Op Op;
typedef struct Op_group Op_group;

struct Op_group {
  UT_array *locals; /* fn_var  */
  char *before;
  char *after;
};

struct Op {
  Plugin *base;
  fn_handle *hndl;
};

void op_new(Plugin *plugin, Buffer *buf, char *arg);
void op_delete(Plugin *plugin);
Op_group* op_newgrp(const char *before, const char *after);
void      op_delgrp(Op_group *);
void      pid_list(List *args);

#endif
