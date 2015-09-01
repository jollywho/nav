#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav.h"

typedef void (*fn)(char **args);

typedef struct {
  String key;
  String name;
  int required_parameters;
  fn exec;
} Cmd;

typedef struct cntlr_s {
  Cmd *cmd_lst;
} Cntlr;

#endif
