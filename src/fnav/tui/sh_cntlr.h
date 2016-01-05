#ifndef FN_TUI_SHELL_CNTLR_H
#define FN_TUI_SHELL_CNTLR_H

#include "fnav/tui/cntlr.h"
#include "fnav/event/shell.h"

typedef struct Sh_cntlr Sh_cntlr;

struct Sh_cntlr {
  Cntlr base;
  Shell *sh;
  fn_handle *hndl;
};

Cntlr* sh_new(Buffer *buf);
void sh_delete(Sh_cntlr *cntlr);

#endif
