/*
** cntlr.h : all rpc calls are run by a controller with a
**           list of cmds.
*/

typedef char* string;

typedef void (*fn)(string *, char **args);

typedef struct cmd_s {
  string name;
  int required_parameters;
  fn exec;
  int argc;
  string argv;
} cmd_t;

typedef struct cntlr_s {
  cmd_t *cmds;
} cntlr_t;
