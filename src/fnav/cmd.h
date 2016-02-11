#ifndef FN_CMD_H
#define FN_CMD_H

#include "fnav/lib/uthash.h"
#include "fnav/cmdline.h"

#define XFILE     0x01
#define BUFFER    0x02
#define FIELD     0x04
#define FUNCTION  0x08
#define RANGE     0x10
#define REGEX     0x20
#define PLUGIN    0x40

typedef struct Cmd_T Cmd_T;
typedef struct Cmdarg Cmdarg;
typedef void* (*Cmd_Func_T)(List *, Cmdarg *);

struct Cmd_T {
  String name;
  Cmd_Func_T cmd_func;
  int flags;
  UT_hash_handle hh;
};

struct Cmdarg {
  int flags;
  Cmdstr *cmdstr;
};

void cmd_add(Cmd_T *cmd);
void cmd_remove(String name);
void cmd_clearall();
void cmd_run(Cmdstr *cmdstr);
Cmd_T* cmd_find(String name);
void cmd_list(List *args);

#endif
