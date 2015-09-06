#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav.h"

typedef struct {
  String name;
  int required_parameters;
  void (*fn)();
} Cmd;

typedef struct Cntlr Cntlr;

struct Cntlr {
  Cmd *cmd_lst;
  void *top;
  void (*_cancel)(Cntlr *cntlr);
  int  (*_input)(Cntlr *cntlr, String key);
};

#endif
