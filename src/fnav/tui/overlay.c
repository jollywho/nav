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
  ov->cntlr_name = strdup("         ");
  ov->cmd_args = strdup("         ");
  ov->color_sep = attr_color("OverlaySep");
  ov->color_line = attr_color("OverlayLine");
  ov->color_namebox = attr_color("OverlayActive");
  return ov;
}

void overlay_delete(Overlay *ov)
{
  free(ov->cntlr_name);
  free(ov->cmd_args);
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

void overlay_edit(Overlay *ov, String name, String label)
{
  log_msg("OVERLAY", "****OV ARGS %s", ov->cmd_args);
  free(ov->cntlr_name);
  free(ov->cmd_args);
  ov->cntlr_name = strdup(name);
  ov->cmd_args = strdup(label);
  window_req_draw(ov, overlay_draw);
}

void overlay_draw(void **argv)
{
  log_msg("OVERLAY", "draw");
  Overlay *ov = argv[0];

  DRAW_STR(ov, nc_win_st, 0, ov->cntlr_name, color_namebox);

  wattron(ov->nc_win_st, COLOR_PAIR(ov->color_line));
  mvwhline(ov->nc_win_st, 0, 9, ' ', ov->ov_size.col);

  if (ov->separator) {
    wattron(ov->nc_win_sep, COLOR_PAIR(ov->color_sep));
    int i;
    for (i = 0; i < ov->ov_size.lnum; i++) {
      mvwaddstr(ov->nc_win_sep, i, 0, "╬");
    }
    wattroff(ov->nc_win_sep, COLOR_PAIR(ov->color_sep));
    DRAW_CH(ov, nc_win_sep, i, 0, ' ', color_line);
  }

  mvwaddstr(ov->nc_win_st, 0, 11, ov->cmd_args);
  wattroff(ov->nc_win_st, COLOR_PAIR(ov->color_line));

  wnoutrefresh(ov->nc_win_st);
  if (ov->separator)
    wnoutrefresh(ov->nc_win_sep);
}
