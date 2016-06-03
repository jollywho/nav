#include "nav/tui/screen.h"
#include "nav/tui/select.h"
#include "nav/model.h"
#include "nav/util.h"
#include "nav/event/fs.h"
#include "nav/option.h"

#define SZ_LEN 6
#define MAX_POS(x) ((x)-(SZ_LEN+1))
static char szbuf[SZ_LEN*2];

static void draw_simple(Buffer *buf, Model *m)
{
  for (int i = 0; i < buf->b_size.lnum; ++i) {
    char *it = model_str_line(m, buf->top + i);
    if (!it)
      break;

    attr_t attr = A_NORMAL;
    if (select_has_line(buf, buf->top + i))
      attr = A_REVERSE;

    int max = MAX_POS(buf->b_size.col);
    draw_wide(buf->nc_win, i, 0, it, max - 1);
    mvwchgat(buf->nc_win, i, 0, -1, attr, buf->col_text, NULL);
  }
}

static void draw_file(Buffer *buf, Model *m)
{
  for (int i = 0; i < buf->b_size.lnum; ++i) {
    char *it = model_str_line(m, buf->top + i);
    if (!it)
      break;

    fn_rec *rec = model_rec_line(m, buf->top + i);

    readable_fs(rec_stsize(rec), szbuf);

    attr_t attr = A_NORMAL;
    if (select_has_line(buf, buf->top + i))
      attr = A_REVERSE;

    int max = MAX_POS(buf->b_size.col);
    draw_wide(buf->nc_win, i, 0, it, max - 1);

    if (isrecreg(rec)) {
      int col = get_syn_colpair(file_ext(it));
      mvwchgat(buf->nc_win, i, 0, -1, attr, col, NULL);
      draw_wide(buf->nc_win, i, 2+max, szbuf, SZ_LEN);
      mvwchgat(buf->nc_win, i, 2+max, -1, attr, buf->col_sz, NULL);
      mvwchgat(buf->nc_win, i, buf->b_size.col - 1, 1, attr, buf->col_text,0);
    }
    else {
      char *symb = "?";
      if (isreclnk(rec))
        symb = ">";
      else if (isrecdir(rec))
        symb = "/";

      mvwchgat(buf->nc_win, i, 0, -1, attr, buf->col_dir, NULL);
      draw_wide(buf->nc_win, i, buf->b_size.col - 1, symb, SZ_LEN);
      mvwchgat(buf->nc_win, i, buf->b_size.col - 1, 1, attr, buf->col_sz, NULL);
    }
  }
}

static void draw_out(Buffer *buf, Model *m)
{
  char *prev = NULL;
  for (int i = 0; i < buf->b_size.lnum; ++i) {
    char *it = model_str_line(m, buf->top + i);
    if (!it)
      break;
    bool alt = false;

    fn_rec *rec = model_rec_line(m, buf->top + i);
    char *fd = rec_fld(rec, "fd");
    short col = *fd == '1' ? opt_color(BUF_STDOUT) : opt_color(BUF_STDERR);
    char *pidstr = rec_fld(rec, "pid");
    sprintf(szbuf, "%s:", pidstr);

    if (prev != pidstr) {
      alt = true;
      prev = pidstr;
    }

    attr_t attr = A_NORMAL;
    if (select_has_line(buf, buf->top + i))
      attr = A_REVERSE;

    int st = alt ? SZ_LEN : 0;
    int max = buf->b_size.col - st;

    if (alt)
      draw_wide(buf->nc_win, i, 0, szbuf, SZ_LEN);
    draw_wide(buf->nc_win, i, st+1, it, max - 2);

    mvwchgat(buf->nc_win, i, 0, -1, A_UNDERLINE, SZ_LEN, NULL);
    mvwchgat(buf->nc_win, i, st, -1, attr, col, NULL);
  }
}

void draw_curline(Buffer *buf)
{
  attr_t attr = A_NORMAL;
  if (select_has_line(buf, buf->lnum + buf->top))
    attr = A_REVERSE;
  mvwchgat(buf->nc_win, buf->lnum, 0, -1, attr, buf->col_focus, NULL);
}

void draw_screen(Buffer *buf)
{
  Model *m = buf->hndl->model;
  switch (buf->scr) {
    case SCR_SIMPLE:
      draw_simple(buf, m);
      draw_curline(buf);
      break;
    case SCR_FILE:
      draw_file(buf, m);
      draw_curline(buf);
      break;
    case SCR_OUT:
      draw_out(buf, m);
      break;
    case SCR_NULL:
      return;
  }
}
