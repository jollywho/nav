#include "nav/tui/overlay.h"
#include "nav/log.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/util.h"

#define SZ_BUFBOX  3
#define SZ_NAMEBOX 8
#define SZ_ARGSBOX 8
#define SZ_LNUMBOX 10
#define ST_USRARG() (SZ_NAMEBOX)
#define ST_PROG() ((SZ_NAMEBOX)-1)
#define ST_LNUMBOX(col) ((col) - ((SZ_ARGSBOX)-1))
#define SZ_USR(col) ((col) - (SZ_BUFBOX + SZ_NAMEBOX + SZ_LNUMBOX))
#define SZ_PROG(col) (SZ_USR(col) + ST_PROG()-2)
#define NAME_FMT " %-"STR(SZ_NAMEBOX)"s"
#define SEPCHAR "╬"

struct Overlay {
  WINDOW *nc_sep;
  WINDOW *nc_st;
  pos_T ov_size;
  pos_T ov_ofs;
  int separator;
  int prog;
  bool queued;
  bool del;
  bool progfin;

  char *usr_arg;
  char *pipe_in;
  char bufno[SZ_BUFBOX];
  char name[SZ_NAMEBOX];
  char lineno[SZ_LNUMBOX];

  short col_name;
  short col_arg;
  short col_sep;
  short col_line;
  short col_text;
  short col_bufno;
  short col_prog;
};

Overlay* overlay_new()
{
  Overlay *ov = malloc(sizeof(Overlay));
  memset(ov, 0, sizeof(Overlay));
  ov->nc_st = newwin(1,1,0,0);
  ov->nc_sep = newwin(1,1,0,0);
  ov->col_sep = opt_color(OVERLAY_SEP);
  ov->col_line = opt_color(OVERLAY_LINE);
  ov->col_text = opt_color(OVERLAY_LINE);
  ov->col_name = opt_color(OVERLAY_ACTIVE);
  ov->col_arg = opt_color(OVERLAY_ARGS);
  ov->col_bufno = opt_color(OVERLAY_BUFNO);
  ov->col_prog = opt_color(OVERLAY_PROGRESS);
  ov->usr_arg = strdup("         ");
  overlay_bufno(ov, 0);

  memset(ov->name,   ' ', SZ_NAMEBOX);
  memset(ov->lineno, ' ', SZ_LNUMBOX);
  return ov;
}

static int overlay_expire(Overlay *ov)
{
  if (ov->del) {
    delwin(ov->nc_sep);
    delwin(ov->nc_st);
    free(ov);
    return 1;
  }
  return 0;
}

void overlay_delete(Overlay *ov)
{
  log_msg("overlay", "delete");
  if (ov->del)
    return;

  if (ov->usr_arg)
    free(ov->usr_arg);
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
  if (*from)
    free(*from);

  *from = strdup(to);
}

void overlay_clear(Overlay *ov)
{
  if (ov->separator) {
    werase(ov->nc_sep);
    wnoutrefresh(ov->nc_sep);
  }
  werase(ov->nc_st);
  wnoutrefresh(ov->nc_st);
}

void overlay_erase(Overlay *ov)
{
  set_string(&ov->usr_arg, "");
  set_string(&ov->pipe_in, "");
  memset(ov->name,   ' ', SZ_NAMEBOX);
  memset(ov->lineno, ' ', SZ_LNUMBOX);
  overlay_clear(ov);
}

void overlay_focus(Overlay *ov)
{
  ov->col_name = opt_color(OVERLAY_ACTIVE);
  ov->col_text = opt_color(OVERLAY_LINE);
  overlay_refresh(ov);
}

void overlay_unfocus(Overlay *ov)
{
  ov->col_name = opt_color(OVERLAY_INACTIVE);
  ov->col_text = opt_color(OVERLAY_TEXTINACTIVE);
  overlay_refresh(ov);
}

void overlay_set(Overlay *ov, pos_T size, pos_T ofs, int sep)
{
  log_msg("OVERLAY", "overlay_set");
  ov->separator = sep;

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
  snprintf(ov->bufno, SZ_BUFBOX, "%02d", id);
}

void overlay_lnum(Overlay *ov, int lnum, int max)
{
  snprintf(ov->lineno, SZ_LNUMBOX, " %*d:%-*d ", 3, lnum+1, 3, max);
  int pos = ST_LNUMBOX(ov->ov_size.col) - 2;
  draw_wide(ov->nc_st, 0, pos, ov->lineno, SZ_ARGSBOX+1);
  mvwchgat (ov->nc_st, 0, pos, -1, A_NORMAL, ov->col_name, NULL);
  wnoutrefresh(ov->nc_st);
}

void overlay_edit(Overlay *ov, char *name, char *usr, char *in)
{
  set_string(&ov->usr_arg, usr);
  set_string(&ov->pipe_in, in);
  if (name)
    snprintf(ov->name, SZ_NAMEBOX, NAME_FMT, name);
  overlay_refresh(ov);
}

void overlay_progress(Overlay *ov, long percent)
{
  log_err("OVERLAY", "prog: %ld", percent);

  int prog = SZ_PROG(ov->ov_size.col) * percent * 0.01;
  if (prog == 0)
    ov->progfin = true;

  //TODO: queue refresh for next draw cycle. not this one. disabled for now.
  ov->progfin = false;
  ov->prog = prog;

  overlay_refresh(ov);
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

  int x = ov->ov_size.col;
  int y = ov->ov_size.lnum;

  draw_wide(ov->nc_st, 0, 0, ov->bufno, SZ_BUFBOX+1);
  mvwchgat (ov->nc_st, 0, 0, SZ_BUFBOX+1, A_NORMAL, ov->col_bufno, NULL);

  draw_wide(ov->nc_st, 0, SZ_BUFBOX-1, ov->name, SZ_NAMEBOX+1);
  mvwchgat (ov->nc_st, 0, SZ_BUFBOX-1, SZ_NAMEBOX+1, A_NORMAL, ov->col_name, NULL);

  mvwhline(ov->nc_st, 0, SZ_NAMEBOX-1, ' ', x);
  mvwchgat(ov->nc_st, 0, SZ_NAMEBOX-1, -1, A_NORMAL, ov->col_line, NULL);

  if (ov->separator) {
    wattron(ov->nc_sep, COLOR_PAIR(ov->col_sep));
    int i;
    for (i = 0; i < y; i++) {
      mvwaddstr(ov->nc_sep, i, 0, SEPCHAR);
    }
    wattroff(ov->nc_sep, COLOR_PAIR(ov->col_sep));
    DRAW_CH(ov, nc_sep, i, 0, ' ', col_line);
  }

  //TODO: if usr_arg exceeds SZ_USR() then compress /*/*/ to fit
  int pos = ST_LNUMBOX(x) - 2;
  draw_wide(ov->nc_st, 0, ST_USRARG(), ov->usr_arg, SZ_USR(x));
  mvwchgat (ov->nc_st, 0, ST_USRARG(), pos, A_NORMAL, ov->col_text, NULL);

  draw_wide(ov->nc_st, 0, pos, ov->lineno, SZ_ARGSBOX+1);
  mvwchgat (ov->nc_st, 0, pos, -1, A_NORMAL, ov->col_name, NULL);

  if (ov->progfin) {
    ov->progfin = false;
    mvwchgat (ov->nc_st, 0, ST_PROG(), SZ_PROG(x), A_REVERSE, ov->col_name, NULL);
  }
  else
    mvwchgat (ov->nc_st, 0, ST_PROG(), ov->prog, A_NORMAL, ov->col_prog, NULL);

  wnoutrefresh(ov->nc_st);
  if (ov->separator)
    wnoutrefresh(ov->nc_sep);
}
