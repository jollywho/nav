#ifndef FN_GUI_WINDOW_H
#define FN_GUI_WINDOW_H

#include "fnav/tui/cntlr.h"

void window_init(void);
void window_req_draw(fn_buf *buf, argv_callback cb);
void window_input(int key);

#endif
