#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav.h"

typedef void (*fn)(char **args);

typedef struct cmd_s {
  string name;
  int required_parameters;
  fn exec;
  int argc;
  string argv;
} cmd_t;

typedef struct cntlr_s {
  cmd_t *cmd_lst;
} cntlr_t;

#endif
