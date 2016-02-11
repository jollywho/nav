#ifndef FN_TUI_LAYOUT_H
#define FN_TUI_LAYOUT_H

#include "fnav/tui/buffer.h"

typedef struct Container Container;
typedef struct {
  Container *root;
  Container *focus;
} Layout;

void layout_init(Layout *layout);
void layout_cleanup(Layout *layout);

void layout_add_buffer(Layout *layout, Buffer *next, enum move_dir dir);
void layout_remove_buffer(Layout *layout);

void layout_movement(Layout *layout, enum move_dir dir);
Buffer* layout_buf(Layout *layout);

void layout_set_status(Layout *layout, String name, String usr, String in);
void layout_refresh(Layout *layout);
void layout_shift(Layout *layout, int lines);

pos_T layout_size();

int layout_is_root(Layout *layout);

#endif
