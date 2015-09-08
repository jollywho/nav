#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav.h"

typedef struct {
  String name;
  int required_parameters;
  void (*fn)();
} Cmd;

typedef struct Cntlr Cntlr;
typedef struct TBL_handle TBL_handle;

struct Cntlr {
  Cmd *cmd_lst;
  TBL_handle *tbl_handle;
  void *top;
  void (*_draw)(Cntlr *cntlr);
  void (*_cancel)(Cntlr *cntlr);
  int  (*_input)(Cntlr *cntlr, String key);
};

#endif
