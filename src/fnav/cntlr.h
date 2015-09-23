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
typedef struct b_focus b_focus;
typedef struct listener listener;

struct fn_handle {
  fn_handle *prev;
  fn_handle *next;
  fn_tbl *tbl;
  fn_buf *buf;
  String fname;
  String fval;
  b_focus *focus;
  listener *listener;
};

struct Cntlr {
  Cmd *cmd_lst;
  fn_handle *hndl;
  void *top;
  void (*_cancel)(Cntlr *cntlr);
  int  (*_input)(Cntlr *cntlr, String key);
};

#endif
