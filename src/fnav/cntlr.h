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
typedef struct fn_buf fn_buf;
typedef struct fn_handle fn_handle;
typedef struct listener listener;

struct fn_handle {
  fn_buf *buf;
  String tname;
  String fname;
  String fval;
  listener *lis;
};

struct Cntlr {
  Cmd *cmd_lst;
  fn_handle *hndl;
  void *top;
  void (*_cancel)(Cntlr *cntlr);
  void (*_focus)(Cntlr *cntlr);
  int  (*_input)(Cntlr *cntlr, int key);
};

#endif
