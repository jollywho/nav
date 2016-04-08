#ifndef FN_CMD_H
#define FN_CMD_H

#include "nav/lib/uthash.h"
#include "nav/cmdline.h"
#include "nav/option.h"

#define OUTPUT    0x01
#define BUFFER    0x02
#define RET_INT   0x04
#define PLUGIN    0x40
#define STRING    0x80
#define NORET (Cmdret){}

typedef struct Cmd_T Cmd_T;
typedef struct Cmdarg Cmdarg;
typedef Cmdret (*Cmd_Func_T)(List *, Cmdarg *);

struct Cmd_T {
  char *name;
  char *alt;
  Cmd_Func_T cmd_func;
  int flags;              /* cmd     flags */
  int bflags;             /* builtin flags */
  UT_hash_handle hh;
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
void cmd_add(const Cmd_T *);
void cmd_remove(const char *);
void cmd_clearall();
void cmd_flush();
void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline);
Cmd_T* cmd_find(const char *);
void cmd_list(List *args);
void cmd_eval(Cmdstr *caller, char *line);
fn_func* cmd_callstack();

#endif
