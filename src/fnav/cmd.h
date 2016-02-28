#ifndef FN_CMD_H
#define FN_CMD_H

#include "fnav/lib/uthash.h"
#include "fnav/cmdline.h"

#define WORD      0x01
#define BUFFER    0x02
#define FIELD     0x04
#define FUNCTION  0x08
#define RANGE     0x10
#define REGEX     0x20
#define PLUGIN    0x40
#define STRING    0x80

typedef struct Cmd_T Cmd_T;
typedef struct Cmdarg Cmdarg;
typedef void* (*Cmd_Func_T)(const List *, Cmdarg *);

struct Cmd_T {
  char *name;
  Cmd_Func_T cmd_func;
  int flags;
  UT_hash_handle hh;
};

struct Cmdarg {
  int flags;
  int pflag;
  Cmdstr *cmdstr;
  Cmdline *cmdline;
};

void cmd_init();
void cmd_cleanup();
void cmd_flush();
void cmd_add(Cmd_T *cmd);
void cmd_remove(const char *);
void cmd_clearall();
void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline);
void* cmd_call(List *args, Cmdarg *ca);
Cmd_T* cmd_find(const char *);
void cmd_list(List *args);
void cmd_eval(char *line);

#endif
