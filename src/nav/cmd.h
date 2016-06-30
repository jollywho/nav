#ifndef NV_CMD_H
#define NV_CMD_H

#include "nav/lib/uthash.h"
#include "nav/cmdline.h"
#include "nav/option.h"

#define OUTPUT    0x01
#define BUFFER    0x02
#define RET_INT   0x04
#define PLUGIN    0x32
#define STRING    0x64
#define NORET (Cmdret){}

typedef struct Cmd_T Cmd_T;
typedef struct Cmdarg Cmdarg;
typedef Cmdret (*Cmd_Func_T)(List *, Cmdarg *);

struct Cmd_T {
  char *name;             /* cmd name      */
  char *alt;              /* alt cmd name  */
  char *msg;
  Cmd_Func_T cmd_func;
  int flags;              /* cmd     flags */
  int bflags;             /* builtin flags */
};

struct Cmdarg {
  int flags;              /* cmd     flags */
  int bflag;              /* builtin flags */
  Cmdstr *cmdstr;
  Cmdline *cmdline;
};

void cmd_init();
void cmd_sort_cmds();
void cmd_cleanup();
void cmd_add(Cmd_T *);
void cmd_remove(const char *);
void cmd_clearall();
void cmd_flush();
void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline);
Cmd_T* cmd_find(const char *);
void cmd_setfile(char *);
void cmd_setline(int);
void cmd_eval(Cmdstr *caller, char *line);
nv_block* cmd_callstack();
void cmd_load(const char *);
void cmd_unload(bool);
bool cmd_conditional();
void cmd_list(List *args);

#endif
