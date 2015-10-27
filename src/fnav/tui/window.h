#ifndef FN_TUI_WINDOW_H
#define FN_TUI_WINDOW_H

#include "fnav/tui/buffer.h"

void window_init(void);
void window_req_draw(Buffer *buf, argv_callback cb);
void window_input(int key);

void window_add_buffer(pos_T dir);
void window_ex_cmd_start();
void window_ex_cmd_end();
void window_draw_all();

#endif
