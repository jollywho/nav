#ifndef FN_TUI_WINDOW_H
#define FN_TUI_WINDOW_H

#include "fnav/tui/buffer.h"

void window_init(void);
void window_req_draw(fn_buf *buf, argv_callback cb);
void window_input(int key);

BufferNode* window_add_buffer();

#endif
