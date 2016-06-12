#ifndef NV_TUI_LAYOUT_H
#define NV_TUI_LAYOUT_H

#include "nav/tui/buffer.h"

typedef struct Container Container;
typedef struct {
  Container *root;
  Container *focus;
} Layout;

void layout_init(Layout *layout);
void layout_cleanup(Layout *layout);

void layout_add_buffer(Layout *layout, Buffer *next, enum move_dir dir);
void layout_remove_buffer(Layout *layout, Buffer *);

void layout_movement(Layout *layout, enum move_dir dir);
void layout_swap(Layout *layout, enum move_dir dir);
Buffer* layout_buf(Layout *layout);

void layout_set_status(Layout *layout, char *, char *, char *);
void layout_refresh(Layout *layout, int);

pos_T layout_size();

int layout_is_root(Layout *layout);
int key2move_type(int);

#endif
