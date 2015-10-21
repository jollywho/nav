#include <ncurses.h>
#include <limits.h>

#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/event/loop.h"
#include "fnav/event/fs.h"

struct fn_buf {
  WINDOW *nc_win;
  BufferNode *bn;
  pos_T b_size;
  pos_T nc_size;
  pos_T b_ofs;

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
  SET_POS(buf->b_size, 0, 0);
  buf->nc_win = newwin(1,1,0,0);
  buf->dirty = false;
  buf->queued = false;
  return buf;
}

void buf_set_size(fn_buf *buf, int w, int h)
{
  log_msg("BUFFER", "SET SIZE %d %d", w, h);
  if (h)
    buf->nc_size.lnum = h;
  if (w)
    buf->nc_size.col = w;
  wresize(buf->nc_win, h, w);
  buf_refresh(buf);
}

void buf_set_ofs(fn_buf *buf, int x, int y)
{
  SET_POS(buf->b_size, y, x);
  mvwin(buf->nc_win, y, x);
}

void buf_destroy(fn_buf *buf)
{
  delwin(buf->nc_win);
  free(buf);
}

void buf_set_cntlr(fn_buf *buf, Cntlr *cntlr)
{
  buf->bn->cntlr = cntlr;
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
  window_req_draw(buf, buf_draw);
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
    //buf_count_rewind(buf, lis);
  }

  pos_T p = {.lnum = 0, .col = 0};
  ventry *it = lis->ent;
  for(int i = 0; i < lis->pos; i++) {
    if (it->head) break;
    it = it->prev;
  }

  log_msg("BUFFER", "druh_start");
  while (1) {
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

int buf_entsize(fn_handle *hndl) {
  return tbl_ent_count(hndl->lis->ent);
}

void buf_mv(fn_buf *buf, int x, int y)
{
  // TODO: convert to ncurses scroll
  if (!buf->hndl->lis->ent) return;
  int *pos = &buf->hndl->lis->pos;
  int *ofs = &buf->hndl->lis->ofs;
  for (int i = 0; i < abs(y); i++) {
    ventry *ent = buf->hndl->lis->ent;
    int d = FN_MV(ent, y);

    if (d == 1) {
      if (*pos < buf->nc_size.lnum * .8)
        (*pos)++;
      else {
        (*ofs)++;
      }
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
