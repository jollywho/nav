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
typedef struct Buffer Buffer;
typedef struct fn_handle fn_handle;
typedef struct Model Model;

struct fn_handle {
  Buffer *buf;
  Model *model;
  String tn;      // listening table name
  String key;     // listening value
  String key_fld; // listening field
  String fname;   // filter field
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
