#ifndef FN_TUI_CNTLR_H
#define FN_TUI_CNTLR_H

#include "fnav/fnav.h"

typedef struct {
  String name;
  int required_parameters;
  void (*fn)();
} Cmd;

typedef void (*argv_callback)(void **argv);
typedef struct Window Window;
typedef struct Cntlr Cntlr;
typedef struct fn_tbl fn_tbl;
typedef struct fn_buf fn_buf;
typedef struct fn_handle fn_handle;
typedef struct fn_lis fn_lis;

struct fn_handle {
  fn_buf *buf;
  String tname;
  String fname;
  String fval;
  fn_lis *lis;
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
