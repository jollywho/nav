// layout
// containers for window tiling
#include <stdlib.h>
#include <sys/ioctl.h>

#include "nav/lib/sys_queue.h"
#include "nav/tui/layout.h"
#include "nav/log.h"

struct Container {
  Buffer *buf;
  pos_T size;
  pos_T ofs;
  enum dir_type dir;
  Container *parent;
  int count;
  int root;
  TAILQ_HEAD(cont, Container) p;
  TAILQ_ENTRY(Container) ent;
};

pos_T layout_size()
{
  struct winsize w;
  ioctl(0, TIOCGWINSZ, &w);
  return (pos_T){w.ws_row, w.ws_col};
}

static Container* create_container(enum move_dir dir, Buffer *buf)
{
  Container *c = calloc(1, sizeof(Container));
  if (dir == MOVE_UP   || dir == MOVE_DOWN )
    c->dir = L_HORIZ;
  if (dir == MOVE_LEFT || dir == MOVE_RIGHT)
    c->dir = L_VERT;

  TAILQ_INIT(&c->p);
  c->buf = buf;
  return c;
}

void layout_init(Layout *layout)
{
  log_msg("LAYOUT", "layout_init");
  Container *root = create_container(MOVE_UP, NULL);
  root->parent = root;
  root->size = layout_size();
  root->size.lnum--;  //cmdline, status
  root->root = 1;
  layout->root = root;
  layout->focus = root;
}

void layout_cleanup(Layout *layout)
{
}

static void layout_update(Container *c)
{
  log_msg("LAYOUT", "update");

  /* allocate divisions by number of items in container */
  int new_lnum = c->size.lnum / MAX(c->count, 1);
  int new_col  = c->size.col  / MAX(c->count, 1);
  int rem_lnum = c->size.lnum % new_lnum;
  int rem_col  = c->size.col  % new_col;

  int i = 0;
  Container *it = TAILQ_FIRST(&c->p);
  while (++i, it) {

    int yfactor = it->dir == L_HORIZ;
    int xfactor = it->dir == L_VERT;

    /* apply divisions to corresponding dimension */
    it->size.lnum = yfactor ? new_lnum : c->size.lnum;
    it->size.col  = xfactor ? new_col  : c->size.col;

    /* apply remainder to last item in container */
    if (i == c->count && yfactor)
      it->size.lnum += rem_lnum;
    if (i == c->count && xfactor)
      it->size.col  += rem_col;

    it->ofs.lnum = c->ofs.lnum + (new_lnum * (i-1) * yfactor);
    it->ofs.col  = c->ofs.col  + (new_col  * (i-1) * xfactor);

    if (TAILQ_EMPTY(&it->p))
      buf_set_size_ofs(it->buf, it->size, it->ofs);
    else
      layout_update(it);

    it = TAILQ_NEXT(it, ent);
  }
}

static void update_sub_items(Container *c)
{
  Container *it = TAILQ_FIRST(&c->p);
  while (it) {
    it->parent = c;
    if (!TAILQ_EMPTY(&it->p))
      update_sub_items(it);

    it = TAILQ_NEXT(it, ent);
  }
}

static void swap_focus(Layout *layout, Container *c)
{
  log_msg("LAYOUT", "swap_focus");
  if (!c) {
    layout->focus = layout->root;
    return;
  }
  if (layout->focus)
    buf_toggle_focus(layout->focus->buf, 0);

  buf_toggle_focus(c->buf, 1);

  layout->focus = c;
}

void layout_add_buffer(Layout *layout, Buffer *nbuf, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_add_buffer");
  Container *focus = layout->focus;
  Container *hc = focus->parent;

  if (focus->root)
    dir = MOVE_UP;

  Container *c = create_container(dir, nbuf);

  /* copy container and leave the old inplace as the parent of subitems */
  if (c->dir != focus->dir ) {
    Container *sub = create_container(dir, focus->buf);
    sub->parent = focus;
    focus->count++;

    TAILQ_INSERT_TAIL(&focus->p, sub, ent);
    hc = focus;
    focus = sub;
  }

  c->parent = hc;
  hc->count++;
  if (!TAILQ_EMPTY(&hc->p))
    TAILQ_INSERT_BEFORE(focus, c, ent);
  else
    TAILQ_INSERT_TAIL(&hc->p, c, ent);

  swap_focus(layout, c);
  layout_update(layout->root);
}

static Container* next_or_prev(Container *it)
{
  return
    TAILQ_NEXT(it, ent) ?
    TAILQ_NEXT(it, ent) :
    TAILQ_PREV(it, cont, ent);
}

static Container* find_container(Container *c, Buffer *buf)
{
  Container *it = TAILQ_FIRST(&c->p);
  while (it) {
    if (!TAILQ_EMPTY(&it->p)) {
      Container *inner = find_container(it, buf);
      if (inner)
        return inner;
    }

    if (it->buf == buf)
      return it;

    it = TAILQ_NEXT(it, ent);
  }
  return NULL;
}

void layout_remove_buffer(Layout *layout, Buffer *buf)
{
  log_msg("LAYOUT", "layout_remove_buffer");
  Container *c = find_container(layout->root, buf);
  Container *hc = c->parent;
  if (!hc)
    return;

  Container *next = next_or_prev(c);
  Container **focus = &layout->focus;
  if (*focus == c || *focus == next) {
    focus = &next;
    layout->focus = NULL;
  }

  TAILQ_REMOVE(&hc->p, c, ent);
  hc->count--;
  free(c);

  if (hc->count == 1 && !hc->root) {
    hc->buf = next->buf;
    hc->count = next->count;
    TAILQ_REMOVE(&hc->p, next, ent);
    TAILQ_SWAP(&next->p, &hc->p, Container, ent);
    free(next);
    next = hc;
    update_sub_items(hc);
  }

  if (next) {
    while (!TAILQ_EMPTY(&next->p))
      next = TAILQ_FIRST(&next->p);
  }

  swap_focus(layout, *focus);
  layout_update(layout->root);
}

static pos_T cur_line(Container *c)
{
  return c->buf ? buf_pos(c->buf) : c->ofs;
}

static pos_T pos_shift(Container *c, enum move_dir dir)
{
  pos_T pos = cur_line(c);

  if (dir == MOVE_LEFT)
    pos = (pos_T){c->ofs.lnum+pos.lnum, c->ofs.col-1};
  else if (dir == MOVE_RIGHT)
    pos = (pos_T){c->ofs.lnum+pos.lnum, c->ofs.col+c->size.col+1};
  else if (dir == MOVE_UP)
    pos = (pos_T){c->ofs.lnum-1, c->ofs.col+1};
  else if (dir == MOVE_DOWN)
    pos = (pos_T){c->ofs.lnum+c->size.lnum+1, c->ofs.col+1};

  return pos;
}

static int intersects(pos_T a, pos_T b, pos_T bsize)
{
  return !(b.col      > a.col  ||
           bsize.col  < a.col  ||
           b.lnum     > a.lnum ||
           bsize.lnum < a.lnum);
}

Container* find_intersect(Container *c, Container *pp, pos_T pos)
{
  log_msg("LAYOUT", "find_intersect");
  Container *it = pp;
  while (it) {
    pos_T it_pos = (pos_T) {
      it->ofs.lnum + it->size.lnum,
      it->ofs.col  + it->size.col };

    int isint = intersects(pos, it->ofs, it_pos);
    if (isint && it != c) {
      if (TAILQ_EMPTY(&it->p))
        return it;

      return find_intersect(c, TAILQ_FIRST(&it->p), pos);
    }
    it = TAILQ_NEXT(it, ent);
  }
  return NULL;
}

void layout_movement(Layout *layout, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_movement");
  log_msg("LAYOUT", "%d", dir);
  Container *c = layout->focus;
  pos_T pos = pos_shift(c, dir);

  Container *pp = NULL;
  pp = find_intersect(c, layout->root, pos);
  if (pp)
    swap_focus(layout, pp);
}

void layout_rotate(Layout *layout, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_rotate");
}

void layout_swap(Layout *layout, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_swap");
  log_msg("LAYOUT", "%d", dir);
  Container *c = layout->focus;
  pos_T pos = pos_shift(c, dir);

  Container *pp = NULL;
  pp = find_intersect(c, layout->root, pos);
  if (!pp)
    return layout_rotate(layout, dir);

  Buffer *swap_buf = pp->buf;
  pp->buf = c->buf;
  c->buf = swap_buf;
  if (c->dir != c->parent->dir)
    c->parent->buf = c->buf;
  if (pp->dir != pp->parent->dir)
    pp->parent->buf = pp->buf;

  layout_update(layout->root);
  swap_focus(layout, pp);
}

Buffer* layout_buf(Layout *layout)
{
  return layout->focus->buf;
}

int layout_is_root(Layout *layout)
{
  return layout->focus == layout->root;
}

void layout_refresh(Layout *layout, int offset)
{
  Container *root = layout->root;
  root->size = layout_size();
  root->size.lnum -= offset;
  layout_update(layout->root);
}

int key2move_type(int key)
{
  switch (key) {
    case 'H': return MOVE_LEFT;
    case 'J': return MOVE_DOWN;
    case 'K': return MOVE_UP;
    case 'L': return MOVE_RIGHT;
    default: return -1;
  }
}
