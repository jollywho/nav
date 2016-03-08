#include "nav/tui/overlay.h"
#include "nav/log.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"

#define STRINGIFY(s) #s
#define STR(s) STRINGIFY(s)
#define SZ_BUFBOX  3
#define SZ_NAMEBOX 8
#define SZ_ARGSBOX 8
#define SZ_LNUMBOX 10
#define ST_USRARG() (SZ_NAMEBOX)
#define ST_ARGBOX(ov) ((ov) - ((SZ_ARGSBOX)-1))
#define NAME_FMT " %-"STR(SZ_NAMEBOX)"s"
#define SEPCHAR "╬"

struct Overlay {
  WINDOW *nc_sep;
  WINDOW *nc_st;
  pos_T ov_size;
  pos_T ov_ofs;
  int separator;
  bool queued;
  bool del;

  char *usr_arg;
  char *pipe_in;
  char bufno[SZ_BUFBOX];
  char name[SZ_NAMEBOX];
  char lineno[SZ_LNUMBOX];

  short color_namebox;
  short color_argsbox;
  short color_sep;
  short color_line;
  short color_text;
  short color_bufno;
};

Overlay* overlay_new()
{
  Overlay *ov = malloc(sizeof(Overlay));
  memset(ov, 0, sizeof(Overlay));
  ov->nc_st = newwin(1,1,0,0);
  ov->nc_sep = newwin(1,1,0,0);
  ov->color_sep = attr_color("OverlaySep");
  ov->color_line = attr_color("OverlayLine");
  ov->color_text = attr_color("OverlayLine");
  ov->color_namebox = attr_color("OverlayActive");
  ov->color_argsbox = attr_color("OverlayArgs");
  ov->color_bufno = attr_color("OverlayBufNo");
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
  ov->color_namebox = attr_color("OverlayActive");
  ov->color_bufno = attr_color("OverlayBufNo");
  ov->color_text = attr_color("OverlayLine");
  overlay_refresh(ov);
}

void overlay_unfocus(Overlay *ov)
{
  ov->color_namebox = attr_color("OverlayInactive");
  ov->color_bufno = attr_color("OverlayInactiveBufNo");
  ov->color_text = attr_color("OverlayTextInactive");
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
  int pos = ST_ARGBOX(ov->ov_size.col) - 2;
  DRAW_STR(ov, nc_st, 0, pos, ov->lineno, color_namebox);
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

void overlay_draw(void **argv)
{
  log_msg("OVERLAY", "draw");
  Overlay *ov = argv[0];
  if (!ov)
    return;
  if (overlay_expire(ov))
    return;
  ov->queued = false;

  DRAW_STR(ov, nc_st, 0, 0, ov->bufno, color_bufno);
  DRAW_STR(ov, nc_st, 0, SZ_BUFBOX-1, ov->name, color_namebox);

  wattron(ov->nc_st, COLOR_PAIR(ov->color_line));
  mvwhline(ov->nc_st, 0, SZ_NAMEBOX-1, ' ', ov->ov_size.col);
  wattroff(ov->nc_st, COLOR_PAIR(ov->color_line));

  if (ov->separator) {
    wattron(ov->nc_sep, COLOR_PAIR(ov->color_sep));
    int i;
    for (i = 0; i < ov->ov_size.lnum; i++) {
      mvwaddstr(ov->nc_sep, i, 0, SEPCHAR);
    }
    wattroff(ov->nc_sep, COLOR_PAIR(ov->color_sep));
    DRAW_CH(ov, nc_sep, i, 0, ' ', color_line);
  }

  DRAW_STR(ov, nc_st, 0, ST_USRARG(), ov->usr_arg, color_text);
  int pos = ST_ARGBOX(ov->ov_size.col) - 2;
  DRAW_STR(ov, nc_st, 0, pos, ov->lineno, color_namebox);

  wnoutrefresh(ov->nc_st);
  if (ov->separator)
    wnoutrefresh(ov->nc_sep);
}
