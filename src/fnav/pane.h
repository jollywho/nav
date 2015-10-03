#ifndef FN_GUI_PANE_H
#define FN_GUI_PANE_H

#include "fnav/cntlr.h"

void pane_init();
void pane_add(Pane *pane, Cntlr *c);
void pane_input(Pane *pane, int key);
void pane_req_draw(fn_buf *buf, argv_callback cb);
#endif
