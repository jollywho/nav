#ifndef FN_TUI_OP_CNTLR_H
#define FN_TUI_OP_CNTLR_H

#include "fnav/event/pty_process.h"

typedef struct Op_cntlr Op_cntlr;

struct Op_cntlr {
  Cntlr base;
  Loop loop;
  fn_handle *hndl;
};

void op_init();
void op_cleanup(Op_cntlr *cntlr);

#endif
