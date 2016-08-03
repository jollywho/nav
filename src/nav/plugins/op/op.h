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
  void *curgrp;  /* nv_group* */
  void *lastgrp; /* nv_group* */
  int last_pid;
  int last_status;
  char *last_line;
};

void op_new(Plugin *plugin, Buffer *buf, char *arg);
void op_delete(Plugin *plugin);

Op_group* op_newgrp(const char *before, const char *after);
void      op_delgrp(Op_group *);

Cmdret op_kill();
void pid_list();
void op_set_exit_status(int);
void op_set_exec_line(char *, void *);
char* op_pid_last();
char* op_status_last();
void* op_active_group();

int op_repeat_last_exec();

#endif
