#ifndef FN_CORE_PANE_H
#define FN_CORE_PANE_H

#include "fnav/cntlr.h"

void pane_init();
void pane_add(Cntlr *c);
void pane_input(int key);
void pane_set_dirty(fn_buf *buf);
#endif
