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

void sh_init(Buffer *buf);
void sh_cleanup(Sh_cntlr *cntlr);

#endif
