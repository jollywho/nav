#include "fnav/tui/overlay.h"
#include "fnav/log.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"

#define COL_ACTIVE 4
#define COL_INACTIVE 6

Overlay* overlay_new()
{
  Overlay *ov = malloc(sizeof(Overlay));
  ov->nc_win_st = newwin(1,1,0,0);
  ov->nc_win_sep = newwin(1,1,0,0);
  ov->color_sep = attr_color("OverlaySep");
  ov->color_line = attr_color("OverlayLine");
  ov->color_namebox = attr_color("OverlayActive");
  ov->name = strdup("         ");
  ov->usr_arg = strdup("         ");
  ov->pipe_in = ov->pipe_out = NULL;
  return ov;
}

void overlay_delete(Overlay *ov)
{
  if (ov->name) free(ov->name);
  if (ov->usr_arg) free(ov->usr_arg);
  if (ov->pipe_in) free(ov->pipe_in);
  if (ov->pipe_out) free(ov->pipe_out);
  delwin(ov->nc_win_sep);
  delwin(ov->nc_win_st);
  free(ov);
}

void overlay_clear(Overlay *ov)
{
  if (ov->separator) {
    werase(ov->nc_win_sep);
    wnoutrefresh(ov->nc_win_sep);
  }
  werase(ov->nc_win_st);
  wnoutrefresh(ov->nc_win_st);
}

void overlay_focus(Overlay *ov)
{
  ov->color_namebox = attr_color("OverlayActive");
  window_req_draw(ov, overlay_draw);
}

void overlay_unfocus(Overlay *ov)
{
  ov->color_namebox = attr_color("OverlayInactive");
  window_req_draw(ov, overlay_draw);
}

void overlay_set(Overlay *ov, Buffer *buf)
{
  log_msg("OVERLAY", "overlay_set");
  pos_T size = buf_size(buf);
  pos_T ofs  = buf_ofs(buf);
  size.lnum--;
  if (ofs.col == 0) {
    ov->separator = 0;
  }
  else {
    ov->separator = 1;
    ofs.col++;
    size.col--;
  }

  overlay_clear(ov);
  log_msg("OVERLAY", "adjust buffer");
  buf_set_size(buf, size);
  buf_set_ofs(buf, ofs);
  ov->ov_size = size;
  ov->ov_ofs = (pos_T){ ofs.lnum + size.lnum , ofs.col };

  wresize(ov->nc_win_st, 1, ov->ov_size.col);
  mvwin(ov->nc_win_st, ov->ov_ofs.lnum, ov->ov_ofs.col);

  wresize(ov->nc_win_sep, size.lnum + 1, 1);
  mvwin(ov->nc_win_sep, ofs.lnum, ofs.col - 1);

  window_req_draw(ov, overlay_draw);
}

static void set_string(String *from, String to)
{
  if (!to) return;
  if (*from) free(*from);
  *from = strdup(to);
}

void overlay_edit(Overlay *ov, String name, String usr, String in, String out)
{
  log_msg("OVERLAY", "****OV ARGS %s %s", name, usr);
  set_string(&ov->name, name);
  set_string(&ov->usr_arg, usr);
  set_string(&ov->pipe_in, in);
  set_string(&ov->pipe_out, out);
  window_req_draw(ov, overlay_draw);
}

void overlay_draw(void **argv)
{
  log_msg("OVERLAY", "draw");
  Overlay *ov = argv[0];

  DRAW_STR(ov, nc_win_st, 0, 0, ov->name, color_namebox);

  wattron(ov->nc_win_st, COLOR_PAIR(ov->color_line));
  mvwhline(ov->nc_win_st, 0, 9, ' ', ov->ov_size.col);
  wattroff(ov->nc_win_st, COLOR_PAIR(ov->color_line));

  if (ov->separator) {
    wattron(ov->nc_win_sep, COLOR_PAIR(ov->color_sep));
    int i;
    for (i = 0; i < ov->ov_size.lnum; i++) {
      mvwaddstr(ov->nc_win_sep, i, 0, "╬");
    }
    wattroff(ov->nc_win_sep, COLOR_PAIR(ov->color_sep));
    DRAW_CH(ov, nc_win_sep, i, 0, ' ', color_line);
  }

  DRAW_STR(ov, nc_win_st, 0, 11, ov->usr_arg, color_line);
  DRAW_STR(ov, nc_win_st, 0, ov->ov_size.col - 5, ov->pipe_out, color_line);

  wnoutrefresh(ov->nc_win_st);
  if (ov->separator)
    wnoutrefresh(ov->nc_win_sep);
}
