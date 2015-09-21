#define _GNU_SOURCE
#include <ncurses.h>
#include <limits.h>

#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/buffer.h"
#include "fnav/loop.h"

typedef struct {
  ventry *ent;
  pos_T pos;
} b_focus;

struct fn_buf {
  WINDOW *nc_win;
  pos_T b_size;
  pos_T nc_size;

  bool inval;

  Job *job;
  JobArg *arg;
  listener *listen;

  fn_handle *hndl;
  String fname;
  b_focus focus;
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
  buf->inval = false;
  SET_POS(buf->focus.pos,0,0);
  SET_POS(buf->b_size,0,0);
  getmaxyx(stdscr, buf->nc_size.lnum, buf->nc_size.col);
  buf->nc_win = newwin(buf->nc_size.lnum, buf->nc_size.col, 0,0);
  buf->job = malloc(sizeof(Job));
  buf->arg = malloc(sizeof(JobArg));
  buf->listen = malloc(sizeof(listener));
  buf->listen->cb = buf_listen;
  buf->listen->buf = buf;
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
  tbl_listener(hndl->tbl, buf->listen, hndl->fname, hndl->fval);
}

void buf_listen(listener *listener)
{
  log_msg("BUFFER", "listen cb");
  fn_buf *buf = listener->buf;
  buf->focus.ent = listener->entry;
  if (buf->inval == false) {
    buf->inval = true;
    QUEUE_PUT(draw, buf->job, buf->arg);
  }
}

void buf_draw(Job *job, JobArg *arg)
{
  log_msg("BUFFER", "$_draw_$");
  fn_buf *buf = job->caller;
#ifndef NCURSES_ENABLED
  String n = (String)rec_fld(buf->focus.ent->rec, buf->fname);
  log_msg("BUFFER", " %s", n);
  buf->inval = false;
  return;
#endif

  wclear(buf->nc_win);
  pos_T p = {.lnum = 0, .col = 0};
  ventry *it = buf->focus.ent;
  for(int i = 0; i < buf->nc_size.lnum; i++) {
    String n = (String)rec_fld(it->rec, buf->fname);
    uv_stat_t *st = (uv_stat_t*)rec_fld(it->rec, "stat");
    if (S_ISDIR(st->st_mode)) {
      // TODO: color attron
    }
    else {
    }
    DRAW_AT(buf->nc_win, p, n);
    it = it->next;
    if (!it->rec) break;
    INC_POS(p,0,1);
  }
  buf->inval = false;
  wrefresh(buf->nc_win);
  refresh();
}

String buf_val(fn_buf *buf, String fname)
{
  return rec_fld(buf->focus.ent->rec, fname);
}

void buf_mv(fn_buf *buf, int x, int y)
{
  if (buf->inval == true) return;

  buf->inval = true;
  for (int i = 0; i < y; i++) {
    FN_MV(buf->focus.ent, next);
  }
  for (int i = 0; i > y; i--) {
    FN_MV(buf->focus.ent, prev);
  }
  QUEUE_PUT(draw, buf->job, buf->arg);
}
