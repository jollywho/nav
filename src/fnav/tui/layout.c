// layout
// buffer tiling management
#include <sys/queue.h>

#include "fnav/tui/layout.h"
#include "fnav/log.h"

enum dir_type { L_HORIZ, L_VERT };

struct Container {
  Buffer *buf;
  pos_T size;
  pos_T ofs;
  enum dir_type dir;
  Container *parent;
  int count;
  TAILQ_HEAD(cont, Container) p;
  TAILQ_ENTRY(Container) ent;
};

pos_T layout_size()
{
  pos_T size;
  getmaxyx(stdscr, size.lnum, size.col);
  return size;
}

static void create_container(Container *c, enum move_dir dir)
{
  if (dir == MOVE_UP   || dir == MOVE_DOWN ) c->dir = L_HORIZ;
  if (dir == MOVE_LEFT || dir == MOVE_RIGHT) c->dir = L_VERT;
  TAILQ_INIT(&c->p);
}

void layout_init(Layout *layout)
{
  log_msg("LAYOUT", "layout_init");
  Container *root = malloc(sizeof(Container));
  root->buf = NULL;
  root->parent = NULL;
  root->size = layout_size();
  root->size.lnum--;  //cmdline
  root->ofs = (pos_T){0,0};
  root->count = 0;
  create_container(root, MOVE_UP);
  layout->c = root;
}

static Container* holding_container(Container *c)
{return c->parent->parent ? c->parent->parent : c->parent;}

static void resize_container(Container *c)
{
  log_msg("LAYOUT", "resize_container");
  int s_y = 1;
  int s_x = 1;
  if (c->dir == L_HORIZ) { s_y = c->count; }
  if (c->dir == L_VERT ) { s_x = c->count; }

  Container *it = TAILQ_FIRST(&c->p);
  while (it != NULL) {
    log_msg("LAYOUT", "___ITAM____");
    int c_w = 0;
    Container *prev = NULL;
    prev = TAILQ_PREV(it, cont, ent);
    if (!prev) {
      prev = c;
    }

    log_msg("LAYOUT", "prev ofs [%d, %d]", prev->ofs.lnum, prev->ofs.col);
    log_msg("LAYOUT", "prev siz [%d, %d]", prev->size.lnum, prev->size.col);

    it->size = (pos_T){
      prev->size.lnum  / (s_y),
      prev->size.col   / (s_x)};

    // add prev size to ofs unless it is the holding container
    if (prev != c) { c_w = 1; }

    it->ofs  = (pos_T){
      prev->ofs.lnum + (prev->size.lnum * (s_y-1) * c_w),
      prev->ofs.col  + (prev->size.col  * (s_x-1) * c_w)};

    log_msg("LAYOUT", "s_y c_w [%d, %d]",  s_y, c_w);
    log_msg("LAYOUT", "s_x c_w [%d, %d]",  s_x, c_w);

    buf_set_ofs(it->buf,  it->ofs);
    buf_set_size(it->buf, it->size);
    it = TAILQ_NEXT(it, ent);
  }
}

void layout_add_buffer(Layout *layout, Buffer *next, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_add_buffer");
  Container *c = malloc(sizeof(Container));
  c->buf = next;
  c->parent = layout->c;
  create_container(c, dir);
  if (c->dir == L_HORIZ) { // new
    log_msg("LAYOUT", "L_HORIZ");
    Container *hc = holding_container(c);
    hc->count++;
    TAILQ_INSERT_TAIL(&hc->p, c, ent);
  }
  if (c->dir == L_VERT) {  // vnew
    log_msg("LAYOUT", "L_VERT");
  }
  resize_container(holding_container(c));
  layout->c = c;
}

void layout_remove_buffer(Layout *layout)
{
  layout->c->buf = NULL;
}

void layout_movement(Layout *layout, enum move_dir dir)
{
}

Buffer* layout_buf(Layout *layout)
{return layout->c->buf;}
