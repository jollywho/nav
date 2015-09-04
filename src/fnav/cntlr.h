#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav.h"

typedef struct {
  String name;
  int required_parameters;
  void (*fn)();
} Cmd;

typedef struct {
  Cmd *cmd_lst; //should be hash
  void (*_cancel)();
  int  (*_input)(String key);
} Cntlr;

#endif
