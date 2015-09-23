#define _GNU_SOURCE
#include <ncurses.h>
#include <limits.h>

#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/buffer.h"
#include "fnav/loop.h"

struct b_focus {
  ventry *ent;
  pos_T pos;
};

struct fn_buf {
  WINDOW *nc_win;
  pos_T b_size;
  pos_T nc_size;

  Job *job;
  JobArg *arg;

  fn_handle *hndl;
  String fname;
};

#define SET_POS(pos,x,y)       \
  (pos.col) = (x);             \
  (pos.lnum) = (y);            \

#define INC_POS(pos,x,y)       \
  (pos.col) += (x);            \
  (pos.lnum) += (y);           \

#define DRAW_AT(win,pos,str)   \
  mvwprintw(win, pos.lnum, pos.col, str);

void buf_listen(listener *listener);

fn_buf* buf_init(fn_handle *hndl)
{
  printf("buf init\n");
  fn_buf *buf = malloc(sizeof(fn_buf));
  hndl->focus = malloc(sizeof(b_focus));
  SET_POS(hndl->focus->pos,0,0);
  SET_POS(buf->b_size,0,0);
  getmaxyx(stdscr, buf->nc_size.lnum, buf->nc_size.col);
  buf->nc_win = newwin(buf->nc_size.lnum, buf->nc_size.col, 0,0);
  buf->job = malloc(sizeof(Job));
  buf->arg = malloc(sizeof(JobArg));
  hndl->listener = malloc(sizeof(listener));
  hndl->listener->cb = buf_listen;
  hndl->listener->hndl = hndl;
  buf->job->hndl = hndl;
  buf->job->caller = buf;
  buf->arg->fn = buf_draw;
  return buf;
}

void buf_set(fn_handle *hndl, String fname)
{
  log_msg("BUFFER", "set");
  fn_buf *buf = hndl->buf;
  buf->fname = fname;
  buf->hndl = hndl;
  tbl_listener(hndl->tbl, hndl->listener, hndl->fname, hndl->fval);
}

void buf_listen(listener *listener)
{
  log_msg("BUFFER", "listen cb");
  fn_buf *buf = listener->hndl->buf;
  listener->hndl->focus->ent = listener->entry;
  QUEUE_PUT(draw, buf->job, buf->arg);
}

void buf_draw(Job *job, JobArg *arg)
{
  log_msg("TABLE", "druh");
  fn_buf *buf = job->caller;

  wclear(buf->nc_win);
  pos_T p = {.lnum = 0, .col = 0};
  b_focus *focus = job->hndl->focus;
  ventry *it = focus->ent;
  for(int i = 0; i < buf->nc_size.lnum; i++) {
    String n = (String)rec_fld(it->rec, buf->fname);
    int *st = (int*)rec_fld(it->rec, "stat");
    if (*st) {
      wattron(buf->nc_win, COLOR_PAIR(1));
      DRAW_AT(buf->nc_win, p, n);
      wattroff(buf->nc_win, COLOR_PAIR(1));
    }
    else {
      wattron(buf->nc_win, COLOR_PAIR(2));
      DRAW_AT(buf->nc_win, p, n);
      wattroff(buf->nc_win, COLOR_PAIR(2));
    }
    wmove(buf->nc_win, focus->pos.lnum, focus->pos.col);
    it = it->next;
    if (!it->rec) break;
    INC_POS(p,0,1);
  }
  wrefresh(buf->nc_win);
  log_msg("TABLE", "_druh");
}

String buf_val(fn_handle *hndl, String fname)
{
  return rec_fld(hndl->focus->ent->rec, fname);
}

void buf_mv(fn_buf *buf, int x, int y)
{
  b_focus *focus = buf->hndl->focus;
  for (int i = 0; i < y; i++) {
    FN_MV(focus->ent, next);
  }
  for (int i = 0; i > y; i--) {
    FN_MV(focus->ent, prev);
  }
  QUEUE_PUT(draw, buf->job, buf->arg);
}
