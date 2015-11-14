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

static Container* holding_container(Container *c, Container *p)
{
  if (c->dir == L_VERT || p->parent == NULL) {
    return p;
  }
  else
    return p->parent;
}

static void resize_container(Container *c)
{
  log_msg("LAYOUT", "resize_container");
  int s_y = 1; int s_x = 1; int os_y = 0; int os_x = 0;
  if (c->dir == L_HORIZ) { s_y = c->count; os_y = 1; }
  if (c->dir == L_VERT ) { s_x = c->count; os_x = 1; }

  int i = 0;
  Container *it = TAILQ_FIRST(&c->p);
  while (++i, it != NULL) {
    log_msg("LAYOUT", "___ITAM____");

    // use prev item in entry to set sizes. otherwise use the parent
    int c_w = 1;
    Container *prev = TAILQ_PREV(it, cont, ent);
    if (!prev) { prev = c; c_w = 0; }

    log_msg("LAYOUT", "prev ofs [%d, %d]", prev->ofs.lnum, prev->ofs.col);
    log_msg("LAYOUT", "prev siz [%d, %d]", prev->size.lnum, prev->size.col);

    int new_lnum = c->size.lnum / s_y;
    int new_col  = c->size.col  / s_x;
    int rem_lnum = c->size.lnum % s_y;
    int rem_col  = c->size.col  % s_x;
    if (i == c->count && (rem_lnum || rem_col)) {
      new_lnum += rem_lnum;
      new_col  += rem_col;
    }
    log_msg("LAYOUT", "new siz [%d, %d]", new_lnum, new_col);
    it->size = (pos_T){ new_lnum, new_col };

    it->ofs  = (pos_T){
      prev->ofs.lnum + (prev->size.lnum * os_y * c_w),
      prev->ofs.col  + (prev->size.col  * os_x * c_w)};

    if (TAILQ_FIRST(&it->p))
      resize_container(it);
    else {
      buf_set_size(it->buf, it->size);
      buf_set_ofs(it->buf,  it->ofs);
    }
    it = TAILQ_NEXT(it, ent);
  }
}

void layout_add_buffer(Layout *layout, Buffer *next, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_add_buffer");
  Container *c = malloc(sizeof(Container));
  c->buf = next;
  create_container(c, dir);
  Container *hc = holding_container(c, layout->c);
  c->parent = hc;
  hc->count++;
  //TODO: if VERT create another container and copy hc contents into it
  TAILQ_INSERT_TAIL(&hc->p, c, ent);
  resize_container(hc);
  layout->c = c;
}

void layout_remove_buffer(Layout *layout)
{
  Container *c = (Container*)layout->c;
  // all of current children must be added to current parent
  resize_container(c);
}

void layout_movement(Layout *layout, enum move_dir dir)
{
}

Buffer* layout_buf(Layout *layout)
{return layout->c->buf;}
