// layout
// buffer tiling management
#include <sys/queue.h>

#include "fnav/tui/layout.h"
#include "fnav/tui/overlay.h"
#include "fnav/log.h"

enum dir_type { L_HORIZ, L_VERT };

struct Container {
  Buffer *buf;
  Overlay *ov;
  pos_T size;
  pos_T ofs;
  enum dir_type dir;
  Container *parent;
  int sub;
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
  c->count = 0;
  c->sub = 0;
  TAILQ_INIT(&c->p);
  c->ov = overlay_new();
}

void layout_init(Layout *layout)
{
  log_msg("LAYOUT", "layout_init");
  Container *root = malloc(sizeof(Container));
  root->buf = NULL;
  root->parent = NULL;
  root->size = layout_size();
  root->size.lnum--;  //cmdline, status
  root->ofs = (pos_T){0,0};
  create_container(root, MOVE_UP);
  root->sub = 1;
  layout->c = root;
}

static Container* holding_container(Container *c, Container *p)
{
  if (p == NULL) return c;
  if (c->dir != p->dir || p->parent == NULL) {return p;}
  else {return p->parent; }
}

static void resize_container(Container *c)
{
  log_msg("LAYOUT", "_*_***resize_container***_*_");
  int s_y = 1; int s_x = 1; int os_y = 0; int os_x = 0;

  int i = 0;
  Container *it = TAILQ_FIRST(&c->p);
  while (++i, it != NULL) {
    if (it->dir == L_HORIZ) { s_y = c->count ; os_y = 1; }
    if (it->dir == L_VERT ) { s_x = c->count ; os_x = 1; }
  log_msg("LAYOUT", "_*_***TYPD %d***_*_", it->dir);

    int new_lnum = c->size.lnum / s_y;
    int new_col  = c->size.col  / s_x;
    int rem_lnum = c->size.lnum % s_y;
    int rem_col  = c->size.col  % s_x;
    if (i == c->count && (rem_lnum || rem_col)) {
      new_lnum += rem_lnum;
      new_col  += rem_col;
    }
    // use prev item in entry to set sizes. otherwise use the parent
    int c_w = 1; int sep = 0;
    Container *prev = TAILQ_PREV(it, cont, ent);
    if (!prev) {
      prev = c; c_w = 0;
    }
    if (it->dir == L_VERT && i == c->count)
      sep = 1;
    it->size = (pos_T){ new_lnum , new_col  };

    it->ofs  = (pos_T){
      prev->ofs.lnum + (prev->size.lnum * os_y * c_w),
      prev->ofs.col  + (prev->size.col  * os_x * c_w)};

    if (TAILQ_EMPTY(&it->p)) {
      buf_set_size(it->buf, it->size);
      buf_set_ofs(it->buf,  it->ofs);
      overlay_set(it->ov, it->buf);
    }
    else {
      overlay_clear(it->ov);
      resize_container(it);
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

  if (!TAILQ_EMPTY(&hc->p))
    TAILQ_INSERT_BEFORE(layout->c, c, ent);
  else
    TAILQ_INSERT_TAIL(&hc->p, c, ent);
  hc->count++;

  if (c->dir != layout->c->dir) {
    Container *clone = malloc(sizeof(Container));
    log_msg("LAYOUT", "##############");
    create_container(clone, dir);
    clone->buf = hc->buf;
    clone->ov = hc->ov;
    clone->parent = hc;
    hc->sub = 1;
    hc->count++;
    TAILQ_INSERT_BEFORE(c, clone, ent);
  }

  Container *hcp = holding_container(hc, hc->parent);
  resize_container(hcp);
  layout->c = c;
}

static Container* next_or_prev(Container *it)
{
  return
    TAILQ_NEXT(it, ent) ?
    TAILQ_NEXT(it,ent) :
    TAILQ_PREV(it, cont, ent);
}

void layout_remove_buffer(Layout *layout)
{
  log_msg("LAYOUT", "layout_remove_buffer");
  Container *c = (Container*)layout->c;
  Container *hc = holding_container(c, c->parent);
  Container *hcp = holding_container(hc, hc->parent);

  /* add all children to container's parent. */
  Container *it = TAILQ_FIRST(&c->p);
  while (it != NULL) {
    TAILQ_CONCAT(&hcp->p, &it->p, ent);
    hc->count--;
    hcp->count++;
    it = TAILQ_NEXT(it, ent);
  }
  Container *next = next_or_prev(c);
  TAILQ_REMOVE(&hc->p, c, ent);
  hc->count--;
  if (hc->count == 1 && hc->dir != next->dir) {
    hc->buf = next->buf;
    hc->ov = next->ov;
    hc->sub = 0;
    hc->count--;
    TAILQ_REMOVE(&hc->p, next, ent);
    next = hc;
  }
  resize_container(hcp);
  layout->c = next;
}

void layout_movement(Layout *layout, enum move_dir dir)
{
  Container *c = layout->c;
  Container *p = TAILQ_PREV(c, cont, ent);
  Container *n = TAILQ_NEXT(c, ent);

  if (dir == MOVE_UP && p)
    layout->c = p;
  if (dir == MOVE_DOWN && n)
    layout->c = n;
}

Buffer* layout_buf(Layout *layout)
{return layout->c->buf;}
