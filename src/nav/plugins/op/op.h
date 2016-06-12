#ifndef NV_PLUGINS_OP_H
#define NV_PLUGINS_OP_H

#include "nav/lib/utarray.h"
#include "nav/plugins/plugin.h"

typedef struct Op Op;
typedef struct Op_group Op_group;

struct Op_group {
  UT_array *locals; /* nv_var  */
  char *before;
  char *after;
};

struct Op {
  Plugin *base;
  Handle *hndl;
  void *curgrp; /* nv_group* */
  int last_pid;
  int last_status;
};

void op_new(Plugin *plugin, Buffer *buf, char *arg);
void op_delete(Plugin *plugin);

Op_group* op_newgrp(const char *before, const char *after);
void      op_delgrp(Op_group *);

Cmdret op_kill();
void pid_list(List *args);
void op_set_exit_status(int);
char* op_pid_last();
char* op_status_last();
void* op_active_group();

#endif
