// layout
// containers for window tiling
#include <sys/ioctl.h>

#include "nav/lib/sys_queue.h"
#include "nav/tui/layout.h"
#include "nav/tui/overlay.h"
#include "nav/log.h"

struct Container {
  Buffer *buf;
  Overlay *ov;
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

static Container* create_container(enum move_dir dir, Overlay *ov, Buffer *buf)
{
  Container *c = calloc(1, sizeof(Container));
  if (dir == MOVE_UP   || dir == MOVE_DOWN )
    c->dir = L_HORIZ;
  if (dir == MOVE_LEFT || dir == MOVE_RIGHT)
    c->dir = L_VERT;
  TAILQ_INIT(&c->p);

  c->ov = ov ? ov : overlay_new();
  c->buf = buf;

  return c;
}

void layout_init(Layout *layout)
{
  log_msg("LAYOUT", "layout_init");
  Container *root = create_container(MOVE_UP, NULL, NULL);
  root->parent = root;
  root->size = layout_size();
  root->size.lnum--;  //cmdline, status
  root->root = 1;
  layout->root = root;
  layout->focus = root;
}

void layout_cleanup(Layout *layout)
{
  // traverse tree, ignore restructuring
  // remove it from tailq
  // overlay_delete it
  // free it
}

static void resize_container(Container *c)
{
  log_msg("LAYOUT", "_*_***resize_container***_*_");
  int s_y = 1; int s_x = 1; int os_y = 0; int os_x = 0;

  int i = 0;
  Container *it = TAILQ_FIRST(&c->p);
  while (++i, it) {
    if (it->dir == L_HORIZ) {
      s_y = c->count;
      os_y = 1;
    }
    if (it->dir == L_VERT ) {
      s_x = c->count;
      os_x = 1;
    }

    int new_lnum = c->size.lnum / s_y;
    int new_col  = c->size.col  / s_x;
    int rem_lnum = c->size.lnum % s_y;
    int rem_col  = c->size.col  % s_x;
    if (i == c->count && (rem_lnum || rem_col)) {
      new_lnum += rem_lnum;
      new_col  += rem_col;
    }
    // use prev item in entry to set sizes. otherwise use the parent
    int c_w = 1;
    Container *prev = TAILQ_PREV(it, cont, ent);
    if (!prev) {
      prev = c;
      c_w = 0;
    }
    it->size = (pos_T) { new_lnum, new_col };

    it->ofs  = (pos_T) {
      prev->ofs.lnum + (prev->size.lnum * os_y * c_w),
      prev->ofs.col  + (prev->size.col  * os_x * c_w)
    };

    if (TAILQ_EMPTY(&it->p)) {
      buf_set_overlay(it->buf, it->ov);
      buf_set_size_ofs(it->buf, it->size, it->ofs);
    }
    else {
      log_err("LAYOUT", "it %p", it->ov);
      overlay_clear(it->ov);
      resize_container(it);
    }
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
  if (layout->focus) {
    buf_toggle_focus(layout->focus->buf, 0);
    overlay_unfocus(layout->focus->ov);
  }
  buf_toggle_focus(c->buf, 1);
  overlay_focus(c->ov);

  layout->focus = c;
}

void layout_add_buffer(Layout *layout, Buffer *nbuf, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_add_buffer");
  Container *focus = layout->focus;
  Container *hc = focus->parent;

  if (focus->root)
    dir = MOVE_UP;

  Container *c = create_container(dir, NULL, nbuf);

  /* copy container and leave the old inplace as the parent of subitems */
  if (c->dir != focus->dir) {
    Container *sub = create_container(dir, focus->ov, focus->buf);
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
  resize_container(layout->root);
}

static Container* next_or_prev(Container *it)
{
  return
    TAILQ_NEXT(it, ent) ?
    TAILQ_NEXT(it, ent) :
    TAILQ_PREV(it, cont, ent);
}

static void remove_overlay(Container *c, Container *hc, Container *n)
{
  if (!n || (c->ov != hc->ov && c->ov != n->ov))
    overlay_delete(c->ov);
}

void layout_remove_buffer(Layout *layout)
{
  log_msg("LAYOUT", "layout_remove_buffer");
  Container *c = layout->focus;
  Container *hc = c->parent;
  if (!hc)
    return;

  Container *next = next_or_prev(c);
  TAILQ_REMOVE(&hc->p, c, ent);
  hc->count--;
  remove_overlay(c, hc, next);
  free(c);
  layout->focus = NULL;

  if (hc->count == 1 && !hc->root) {
    remove_overlay(hc, hc->parent, next);
    hc->buf = next->buf;
    hc->ov = next->ov;
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

  swap_focus(layout, next);
  resize_container(layout->root);
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

void layout_swap(Layout *layout, enum move_dir dir)
{
  log_msg("LAYOUT", "layout_swap");
  log_msg("LAYOUT", "%d", dir);
  Container *c = layout->focus;
  pos_T pos = pos_shift(c, dir);

  Container *pp = NULL;
  pp = find_intersect(c, layout->root, pos);
  if (!pp)
    return;

  Buffer *swap_buf = pp->buf;
  Overlay *swap_ov = pp->ov;
  pp->buf = c->buf;
  pp->ov = c->ov;
  c->buf = swap_buf;
  c->ov = swap_ov;
  buf_set_overlay(pp->buf, pp->ov);
  buf_set_overlay(c->buf, c->ov);
  if (c->dir != c->parent->dir) {
    c->parent->buf = c->buf;
    c->parent->ov = c->ov;
  }
  if (pp->dir != pp->parent->dir) {
    pp->parent->buf = pp->buf;
    pp->parent->ov = pp->ov;
  }

  resize_container(layout->root);
  swap_focus(layout, pp);
}

Buffer* layout_buf(Layout *layout)
{
  return layout->focus->buf;
}

void layout_set_status(Layout *layout, char *name, char *usr, char *in)
{
  overlay_edit(layout->focus->ov, name, usr, in);
}

int layout_is_root(Layout *layout)
{
  return layout->focus == layout->root;
}

void layout_refresh(Layout *layout, int offset)
{
  Container *c = layout->root;
  c->size = layout_size();
  c->size.lnum -= offset;
  c->ofs = (pos_T){0,0};
  resize_container(c);
}

int key2move_type(int key)
{
  if (key == 'H')
    return MOVE_LEFT;
  else if (key == 'J')
    return MOVE_DOWN;
  else if (key == 'K')
    return MOVE_UP;
  else if (key == 'L')
    return MOVE_RIGHT;
  else
    return -1;
}
