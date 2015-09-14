#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav/fnav.h"

typedef struct {
  String name;
  int required_parameters;
  void (*fn)();
} Cmd;

typedef struct Cntlr Cntlr;
typedef struct fn_tbl fn_tbl;

struct Cntlr {
  Cmd *cmd_lst;
  fn_tbl *tbl;
  void *top;
  void (*_draw)(Cntlr *cntlr);
  void (*_cancel)(Cntlr *cntlr);
  int  (*_input)(Cntlr *cntlr, String key);
};

#endif
