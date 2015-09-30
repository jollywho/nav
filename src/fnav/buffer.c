#define _GNU_SOURCE
#include <ncurses.h>
#include <limits.h>

#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/buffer.h"
#include "fnav/loop.h"
#include "fnav/fs.h"

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

void buf_listen(fn_handle *hndl);

fn_buf* buf_init()
{
  printf("buf init\n");
  fn_buf *buf = malloc(sizeof(fn_buf));
  SET_POS(buf->b_size,0,0);
  getmaxyx(stdscr, buf->nc_size.lnum, buf->nc_size.col);
  buf->nc_win = newwin(buf->nc_size.lnum, buf->nc_size.col, 0,0);
  buf->job = malloc(sizeof(Job));
  buf->arg = malloc(sizeof(JobArg));
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
  buf->job->hndl = hndl;
  tbl_listener(hndl, buf_listen);
}

void buf_listen(fn_handle *hndl)
{
  log_msg("BUFFER", "listen cb");
  QUEUE_PUT(draw, hndl->buf->job, hndl->buf->arg);
}

void buf_draw(Job *job, JobArg *arg)
{
  log_msg("BUFFER", "druh");
  fn_buf *buf = job->caller;

  wclear(buf->nc_win);
  pos_T p = {.lnum = 0, .col = 0};
  listener *lis = buf->hndl->lis;
  ventry *it = lis->ent;
  if (!it) return;
  int i;
  for(i = 0; i < lis->pos; i++) {
    if (it->prev->head) break;
    it = it->prev;
    if (!it->rec) break;
  }
  log_msg("BUFFER", "___[ofs %d]__", lis->ofs);
  log_msg("BUFFER", "___[pos %d]__", lis->pos);
  log_msg("BUFFER", "___[count %d]__", i);
  for(int i = 0; i < buf->nc_size.lnum; i++) {
    if (!it->rec) break;
    String n = (String)rec_fld(it->rec, buf->fname);
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
    if (it->prev->head) break;
    INC_POS(p,0,1);
  }
  wmove(buf->nc_win, lis->pos, 0);
  wchgat(buf->nc_win, -1, A_REVERSE, 1, NULL);
  //TODO: use wnoutrefresh here and refresh once on timer
  wnoutrefresh(buf->nc_win);
}

String buf_val(fn_handle *hndl, String fname)
{
  return rec_fld(hndl->lis->ent->rec, fname);
}

fn_rec* buf_rec(fn_handle *hndl)
{
  return hndl->lis->ent->rec;
}

int buf_pgsize(fn_handle *hndl)
{
  return hndl->buf->nc_size.col / 3;
}

void buf_mv(fn_buf *buf, int x, int y)
{
  int *pos = &buf->hndl->lis->pos;
  int *ofs = &buf->hndl->lis->ofs;
  for (int i = 0; i < abs(y); i++) {
    ventry *ent = buf->hndl->lis->ent;
    int d = FN_MV(ent, y);
    if (d == 1) {
      buf->hndl->lis->ent = ent->next;
      if (*pos < buf->nc_size.lnum * .8)
        (*pos)++;
      else
        (*ofs)++;
    }
    if (d == -1) {
      buf->hndl->lis->ent = ent->prev;
      if (*pos > buf->nc_size.lnum * .2)
        (*pos)--;
      else
        if (*ofs > 0)
          (*ofs)--;
        else
          (*pos)--;
    }
  }
  QUEUE_PUT(draw, buf->job, buf->arg);
}
