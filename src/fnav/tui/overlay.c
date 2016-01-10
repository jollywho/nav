#include "fnav/tui/overlay.h"
#include "fnav/log.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"

#define STRINGIFY(s) #s
#define STR(s) STRINGIFY(s)
#define SZ_NAMEBOX 10
#define SZ_ARGSBOX 8
#define ST_USRARG() (SZ_NAMEBOX)
#define ST_ARGBOX(ov) ((ov) - ((SZ_ARGSBOX)-1))
#define NAME_FMT "%-"STR(SZ_NAMEBOX)"s"
#define SEPCHAR "╬"

struct Overlay {
  WINDOW *nc_sep;
  WINDOW *nc_st;
  pos_T ov_size;
  pos_T ov_ofs;
  int separator;
  bool queued;
  bool del;

  String usr_arg;
  String pipe_in;
  char name[SZ_NAMEBOX];
  char pipe_out[SZ_ARGSBOX];

  int color_namebox;
  int color_argsbox;
  int color_sep;
  int color_line;
  int color_text;
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
  ov->usr_arg = strdup("         ");
  memset(ov->name,     ' ', SZ_NAMEBOX);
  memset(ov->pipe_out, ' ', SZ_ARGSBOX);
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
  if (ov->usr_arg) free(ov->usr_arg);
  if (ov->pipe_in) free(ov->pipe_in);

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

void overlay_clear(Overlay *ov)
{
  if (ov->separator) {
    werase(ov->nc_sep);
    wnoutrefresh(ov->nc_sep);
  }
  werase(ov->nc_st);
  wnoutrefresh(ov->nc_st);
}

void overlay_focus(Overlay *ov)
{
  ov->color_namebox = attr_color("OverlayActive");
  ov->color_text = attr_color("OverlayLine");
  overlay_refresh(ov);
}

void overlay_unfocus(Overlay *ov)
{
  ov->color_namebox = attr_color("OverlayInactive");
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

static void set_string(String *from, String to)
{
  if (!to) return;
  if (*from) free(*from);
  *from = strdup(to);
}

void overlay_edit(Overlay *ov, String name, String usr, String in, String out)
{
  set_string(&ov->usr_arg, usr);
  set_string(&ov->pipe_in, in);
  if (name)
    snprintf(ov->name, SZ_NAMEBOX, NAME_FMT, name);
  if (out)
    snprintf(ov->pipe_out, SZ_ARGSBOX, "   |>%-2s", out);
  overlay_refresh(ov);
}

void overlay_draw(void **argv)
{
  log_msg("OVERLAY", "draw");
  Overlay *ov = argv[0];
  if (!ov) return;
  if (overlay_expire(ov)) return;
  ov->queued = false;

  DRAW_STR(ov, nc_st, 0, 0, ov->name, color_namebox);

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
  DRAW_STR(ov, nc_st, 0, ST_ARGBOX(ov->ov_size.col), ov->pipe_out, color_argsbox);

  wnoutrefresh(ov->nc_st);
  if (ov->separator)
    wnoutrefresh(ov->nc_sep);
}
