#ifndef FN_TUI_LAYOUT_H
#define FN_TUI_LAYOUT_H

#include "fnav/tui/buffer.h"

void layout_balance(BufferNode *focus, int count, pos_T dir);
void layout_buffer_from_curs(BufferNode *bn, pos_T dir);

pos_T layout_size();

#endif
