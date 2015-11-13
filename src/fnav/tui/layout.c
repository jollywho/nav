// layout
// buffer tiling management
#include <ncurses.h>
#include "fnav/lib/utarray.h"

#include "fnav/tui/layout.h"
#include "fnav/log.h"

enum dir_type { L_HORIZ, L_VERT };

struct Container {
  Buffer *buf;
  pos_T size;
  pos_T ofs;
  enum dir_type dir;
  UT_array *items;
};

void cont_copy(void *_dst, const void *_src) {
  Container *dst = (Container*)_dst, *src = (Container*)_src;
  log_msg("LAYOUT", "CONT %d %d", src->size.col, src->ofs.lnum);
  dst->buf = src->buf;
  dst->size = src->size;
  dst->ofs = src->ofs;
  dst->dir = src->dir;
  dst->items = dst->items;
}
static UT_icd c_icd = {sizeof(Container),NULL,cont_copy,NULL};

pos_T layout_size()
{
  pos_T size;
  getmaxyx(stdscr, size.lnum, size.col);
  return size;
}

static void create_container(Container *c, enum move_dir dir)
{
  if (dir == MOVE_UP   || dir == MOVE_DOWN)  c->dir = L_HORIZ;
  if (dir == MOVE_LEFT || dir == MOVE_RIGHT) c->dir = L_VERT;
  utarray_new(c->items, &c_icd);
}

void layout_init(Layout *layout)
{
  log_msg("LAYOUT", "layout_init");
  Container *root = malloc(sizeof(Container));
  root->buf = NULL;
  root->dir = L_HORIZ;
  root->size = layout_size();
  root->size.lnum--;  //cmdline
  root->ofs = (pos_T){0,0};
  create_container(root, MOVE_LEFT);
  layout->c = root;
}

static void resize_container(Container *c)
{
  Container *it = NULL;
  while ((it = (Container*)utarray_next(c->items, it))) {
    buf_set_ofs(it->buf, c->ofs);
    buf_set_size(it->buf, c->size);
  }
}

void layout_add_buffer(Layout *layout, Buffer *next, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_add_buffer");
  Container *c = malloc(sizeof(Container));
  c->buf = next;
  create_container(c, dir);
  //FIXME: decide contain by type
  utarray_push_back(layout->c->items, c);
  c->size = (pos_T){
    layout->c->size.lnum / (utarray_len(layout->c->items)),
    layout->c->size.col  / (utarray_len(layout->c->items))};
  c->ofs  = (pos_T){
    layout->c->ofs.lnum + c->size.lnum * utarray_eltidx(c->items,&c),
    layout->c->ofs.col  + c->size.col  * utarray_eltidx(c->items,&c)};
  resize_container(layout->c);
  layout->c = c;
}

void layout_remove_buffer(Layout *layout)
{
}

void layout_movement(Layout *layout, enum move_dir dir)
{
}

Buffer* layout_buf(Layout *layout)
{return layout->c->buf;}
