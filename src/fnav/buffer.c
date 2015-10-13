#define _GNU_SOURCE
#include <ncurses.h>
#include <limits.h>

#include "fnav/pane.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/buffer.h"
#include "fnav/loop.h"
#include "fnav/fs.h"

struct fn_buf {
  WINDOW *nc_win;
  Pane *pane;
  pos_T b_size;
  pos_T nc_size;

  fn_handle *hndl;
  String vname;
  bool dirty;
  bool queued;
};

#define SET_POS(pos,x,y)       \
  (pos.col) = (x);             \
  (pos.lnum) = (y);            \

#define INC_POS(pos,x,y)       \
  (pos.col) += (x);            \
  (pos.lnum) += (y);           \

#define DRAW_AT(win,pos,str)   \
  mvwprintw(win, pos.lnum, pos.col, str);

void buf_listen(fn_handle *hndl);
void buf_draw(void **argv);

fn_buf* buf_init()
{
  log_msg("INIT", "BUFFER");
  fn_buf *buf = malloc(sizeof(fn_buf));
  SET_POS(buf->b_size,0,0);
  getmaxyx(stdscr, buf->nc_size.lnum, buf->nc_size.col);
  buf->nc_win = newwin(buf->nc_size.lnum, buf->nc_size.col, 0,0);
  buf->dirty = false;
  buf->queued = false;
  return buf;
}

void buf_set(fn_handle *hndl, String fname)
{
  log_msg("BUFFER", "set");
  fn_buf *buf = hndl->buf;
  buf->vname = fname;
  buf->hndl = hndl;
  tbl_listener(hndl, buf_listen);
}

void buf_resize(fn_buf *buf, int w, int h)
{
  SET_POS(buf->b_size, w, h);
}

void buf_refresh(fn_buf *buf)
{
  log_msg("BUFFER", "refresh");
  if (buf->queued)
    return;
  buf->queued = true;
  pane_req_draw(buf, buf_draw);
}

void buf_listen(fn_handle *hndl)
{
  log_msg("BUFFER", "listen cb");
  hndl->buf->dirty = true;
  buf_refresh(hndl->buf);
}

void buf_count_rewind(fn_buf *buf, fn_lis *lis)
{
  log_msg("BUFFER", "rewind");
  // TODO: rewind running when attaching lis. want it after insert finishes
  // NOTE: pos + ofs = absolute
  ventry *it = lis->ent;
  int count, sub;
  log_msg("BUFFER", "rewind at %d", lis->pos);
  for(count = 0, sub = 0; count < lis->pos; count++, sub++) {
    log_msg("BUFFER", "head? %d %d", it->head, count);
    if (it->head) break;
    log_msg("BUFFER", "druh_nexnd");
    it = it->prev;
    if (lis->ofs > 0 && sub > buf->nc_size.lnum) {
      sub = 0;
      lis->ofs--;
    }
  }
  lis->pos = count;
}

void buf_draw(void **argv)
{
  log_msg("BUFFER", "druh");
  fn_buf *buf = (fn_buf*)argv[0];
  fn_lis *lis = buf->hndl->lis;

  wclear(buf->nc_win);
  if (!lis->ent) {
    log_msg("BUFFER", "druh NOP");
    buf->dirty = false;
    buf->queued = false;
    wnoutrefresh(buf->nc_win);
    return;
  }

  if (buf->dirty) {
    buf_count_rewind(buf, lis);
  }

  pos_T p = {.lnum = 0, .col = 0};
  ventry *it = lis->ent;
  for(int i = 0; i < lis->pos; i++) {
    if (it->head) break;
    it = it->prev;
  }

  log_msg("BUFFER", "druh_start");
  for(int i = 0; i < buf->nc_size.lnum; i++) {
    String n = (String)rec_fld(it->rec, buf->vname);
    if (isdir(it->rec)) {
      wattron(buf->nc_win, COLOR_PAIR(1));
      DRAW_AT(buf->nc_win, p, n);
      wattroff(buf->nc_win, COLOR_PAIR(1));
    }
    else {
      wattron(buf->nc_win, COLOR_PAIR(2));
      DRAW_AT(buf->nc_win, p, n);
      wattroff(buf->nc_win, COLOR_PAIR(2));
    }
    it = it->next;
    if (it->head) break;
    INC_POS(p,0,1);
  }
  log_msg("BUFFER", "druh_end");
  wmove(buf->nc_win, lis->pos, 0);
  wchgat(buf->nc_win, -1, A_REVERSE, 1, NULL);
  wnoutrefresh(buf->nc_win);
  buf->dirty = false;
  buf->queued = false;
}

String buf_val(fn_handle *hndl, String fname)
{
  if (!hndl->lis->ent) return NULL;
  return rec_fld(hndl->lis->ent->rec, fname);
}

fn_rec* buf_rec(fn_handle *hndl)
{
  if (!hndl->lis->ent) return NULL;
  return hndl->lis->ent->rec;
}

int buf_pgsize(fn_handle *hndl) {
  return hndl->buf->nc_size.col / 3;
}

// my guess is the RB tree is rebalancing itself on insertion
// which breaks stale pointers, like every entry!
// this doesnt explain tho why it only breaks moving left dirs.
int buf_entsize(fn_handle *hndl) {
  log_msg("BUFFER", "|||COUNT %p|||", hndl->lis->ent->val);
  return hndl->lis->ent->val->count;
}

void buf_mv(fn_buf *buf, int x, int y)
{
  if (!buf->hndl->lis->ent) return;
  int *pos = &buf->hndl->lis->pos;
  int *ofs = &buf->hndl->lis->ofs;
  for (int i = 0; i < abs(y); i++) {
    ventry *ent = buf->hndl->lis->ent;
    int d = FN_MV(ent, y);

    if (d == 1) {
      if (*pos < buf->nc_size.lnum * .8)
        (*pos)++;
      else
        (*ofs)++;
      buf->hndl->lis->ent = ent->next;
    }
    else if (d == -1) {
      if (*pos > buf->nc_size.lnum * .2)
        (*pos)--;
      else {
        if (*ofs > 0)
          (*ofs)--;
        else
          (*pos)--;
      }
      buf->hndl->lis->ent = ent->prev;
    }
  }
  buf_refresh(buf);
}
