#include "nav/tui/overlay.h"
#include "nav/log.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/util.h"

#define SZ_BUF      3
#define SZ_LBL      8
#define SZ_ARGS     8
#define SZ_LN       10
#define ST_ARG()    (SZ_LBL)
#define ST_PRG()    ((SZ_LBL)-1)
#define ST_LN(col)  ((col)-((SZ_ARGS)-1))
#define SZ_USR(col) ((col)-(SZ_BUF+SZ_LBL+SZ_LN))
#define SZ_PRG(col) (SZ_USR(col)+ST_PRG()-2)
#define LBL_FMT     " %-"STR(SZ_LBL)"s"

struct Overlay {
  WINDOW *nc_sep;
  WINDOW *nc_st;
  pos_T ov_size;
  pos_T ov_ofs;
  int separator;
  int prog;
  int filter;
  bool queued;
  bool del;
  bool progfin;
  bool redraw;

  char *arg;
  char *pipe_in;
  char bufno[SZ_BUF];
  char name[SZ_LBL];
  char lineno[SZ_LN];

  short col_lbl;
  short col_text;
  short col_ln;
  short col_arg;
  short col_sep;
  short col_bufno;
  short col_prog;
  short col_fil;
};

static char *sep_char;

Overlay* overlay_new()
{
  Overlay *ov = malloc(sizeof(Overlay));
  memset(ov, 0, sizeof(Overlay));
  ov->nc_st  = newwin(1,1,0,0);
  ov->nc_sep = newwin(1,1,0,0);
  leaveok(ov->nc_st,  true);
  leaveok(ov->nc_sep, true);

  ov->col_lbl   = opt_color(OVERLAY_ACTIVE);
  ov->col_text  = opt_color(OVERLAY_LINE);
  ov->col_ln    = opt_color(OVERLAY_LINE);
  ov->col_sep   = opt_color(OVERLAY_SEP);
  ov->col_arg   = opt_color(OVERLAY_ARGS);
  ov->col_bufno = opt_color(OVERLAY_BUFNO);
  ov->col_prog  = opt_color(OVERLAY_PROGRESS);
  ov->col_fil   = opt_color(OVERLAY_FILTER);

  sep_char  = get_opt_str("sepchar");

  ov->arg = strdup("         ");
  overlay_bufno(ov, 0);

  memset(ov->name,   ' ', SZ_LBL);
  memset(ov->lineno, ' ', SZ_LN);
  return ov;
}

static int overlay_expire(Overlay *ov)
{
  if (!ov->del)
    return 0;

  delwin(ov->nc_sep);
  delwin(ov->nc_st);
  free(ov);
  return 1;
}

void overlay_delete(Overlay *ov)
{
  log_msg("overlay", "delete");
  if (ov->del)
    return;

  if (ov->arg)
    free(ov->arg);
  if (ov->pipe_in)
    free(ov->pipe_in);

  ov->del = true;
  if (!ov->queued)
    overlay_expire(ov);
}

static void overlay_refresh(Overlay *ov)
{
  if (ov->queued)
    return;
  ov->queued = true;
  window_req_draw(ov, overlay_draw);
}

static void set_string(char **from, char *to)
{
  if (!to)
    return;
  free(*from);

  *from = strdup(to);
}

void overlay_clear(Overlay *ov)
{
  werase(ov->nc_sep);
  wnoutrefresh(ov->nc_sep);
  werase(ov->nc_st);
  wnoutrefresh(ov->nc_st);
}

void overlay_erase(Overlay *ov)
{
  if (ov->del)
    return;
  set_string(&ov->arg, "");
  set_string(&ov->pipe_in, "");
  memset(ov->name,   ' ', SZ_LBL);
  memset(ov->lineno, ' ', SZ_LN);
  overlay_clear(ov);
}

void overlay_focus(Overlay *ov)
{
  ov->col_lbl = opt_color(OVERLAY_ACTIVE);
  ov->col_text = opt_color(OVERLAY_LINE);
  overlay_refresh(ov);
}

void overlay_unfocus(Overlay *ov)
{
  ov->col_lbl = opt_color(OVERLAY_INACTIVE);
  ov->col_text = opt_color(OVERLAY_TEXTINACTIVE);
  overlay_refresh(ov);
}

void overlay_set(Overlay *ov, pos_T size, pos_T ofs, int sep)
{
  log_msg("OVERLAY", "overlay_set");
  ov->separator = sep;
  ov->redraw = true;

  overlay_clear(ov);
  ov->ov_size = (pos_T){size.lnum, size.col};
  ov->ov_ofs  = (pos_T){ofs.lnum + size.lnum, ofs.col };

  wresize(ov->nc_st, 1, ov->ov_size.col);
  mvwin(ov->nc_st, ov->ov_ofs.lnum, ov->ov_ofs.col);

  wresize(ov->nc_sep, size.lnum + 1, 1);
  mvwin(ov->nc_sep, ofs.lnum, ofs.col - 1);

  overlay_refresh(ov);
}

void overlay_bufno(Overlay *ov, int id)
{
  snprintf(ov->bufno, SZ_BUF, "%02d", id);
}

void overlay_lnum(Overlay *ov, int lnum, int max)
{
  snprintf(ov->lineno, SZ_LN, " %*d:%-*d ", 3, lnum+1, 3, max);
  int pos = ST_LN(ov->ov_size.col) - 2;
  draw_wide(ov->nc_st, 0, pos, ov->lineno, SZ_ARGS+1);
  mvwchgat (ov->nc_st, 0, pos,  -1, A_NORMAL, ov->col_lbl, NULL);
  mvwchgat (ov->nc_st, 0, pos+5, ov->filter, A_NORMAL, ov->col_fil, NULL);
  wnoutrefresh(ov->nc_st);
}

void overlay_filter(Overlay *ov, int max, bool enable)
{
  if (!enable) {
    ov->filter = 0;
    return;
  }
  char szbuf[4];
  snprintf(szbuf, 4, "%d", max);
  ov->filter = strlen(szbuf);
}

void overlay_edit(Overlay *ov, char *name, char *usr, char *in)
{
  log_msg("OVERLAY", "edit: %s ", name);
  set_string(&ov->arg, usr);
  set_string(&ov->pipe_in, in);
  if (name)
    snprintf(ov->name, SZ_LBL, LBL_FMT, name);
  overlay_refresh(ov);
}

void overlay_progress(Overlay *ov, long percent)
{
  log_err("OVERLAY", "prog: %ld", percent);

  int prog = SZ_PRG(ov->ov_size.col) * percent * 0.01;
  if (prog == 0)
    ov->progfin = true;

  //TODO: queue refresh for next draw cycle. not this one. disabled for now.
  ov->progfin = false;
  ov->prog = prog;

  overlay_refresh(ov);
}

static void overlay_draw_separator(Overlay *ov)
{
  ov->redraw = false;
  int y = ov->ov_size.lnum;
  wattron(ov->nc_sep, COLOR_PAIR(ov->col_sep));

  int i;
  for (i = 0; i < y; i++)
    mvwaddstr(ov->nc_sep, i, 0, sep_char);

  wattroff(ov->nc_sep, COLOR_PAIR(ov->col_sep));
  DRAW_CH(ov, nc_sep, i, 0, ' ', col_ln);
  wnoutrefresh(ov->nc_sep);
}

static void overlay_draw_progress(Overlay *ov)
{
  int x = ov->ov_size.col;
  if (ov->progfin) {
    ov->progfin = false;
    mvwchgat(ov->nc_st, 0, ST_PRG(), SZ_PRG(x), A_REVERSE, ov->col_lbl, NULL);
  }
  else
    mvwchgat(ov->nc_st, 0, ST_PRG(), ov->prog, A_NORMAL, ov->col_prog, NULL);
}

static void overlay_draw_labels(Overlay *ov)
{
  int x = ov->ov_size.col;
  draw_wide(ov->nc_st, 0, 0, ov->bufno, SZ_BUF+1);
  mvwchgat (ov->nc_st, 0, 0, SZ_BUF+1, A_NORMAL, ov->col_bufno, NULL);

  draw_wide(ov->nc_st, 0, SZ_BUF-1, ov->name, SZ_LBL+1);
  mvwchgat (ov->nc_st, 0, SZ_BUF-1, SZ_LBL+1, A_NORMAL, ov->col_lbl, NULL);

  mvwhline(ov->nc_st, 0, SZ_LBL-1, ' ', x);
  mvwchgat(ov->nc_st, 0, SZ_LBL-1, -1, A_NORMAL, ov->col_ln, NULL);

  //TODO: if arg exceeds SZ_USR() then compress /*/*/ to fit
  int pos = ST_LN(x) - 2;
  draw_wide(ov->nc_st, 0, ST_ARG(), ov->arg, SZ_USR(x));
  mvwchgat (ov->nc_st, 0, ST_ARG(), pos, A_NORMAL, ov->col_text, NULL);

  draw_wide(ov->nc_st, 0, pos, ov->lineno, SZ_ARGS+1);
  mvwchgat (ov->nc_st, 0, pos,  -1, A_NORMAL, ov->col_lbl, NULL);
  mvwchgat (ov->nc_st, 0, pos+5, ov->filter, A_NORMAL, ov->col_fil, NULL);
}

void overlay_draw(void **argv)
{
  log_msg("OVERLAY", "draw");
  Overlay *ov = argv[0];

  if (!ov)
    return;
  if (overlay_expire(ov))
    return;

  ov->queued = false;

  if (ov->redraw && ov->separator)
    overlay_draw_separator(ov);

  overlay_draw_labels(ov);
  overlay_draw_progress(ov);

  wnoutrefresh(ov->nc_st);
}
