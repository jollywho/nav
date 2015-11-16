#include "fnav/tui/overlay.h"
#include "fnav/log.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"

Overlay* overlay_new()
{
  Overlay *ov = malloc(sizeof(Overlay));
  ov->nc_win_st = newwin(1,1,0,0);
  ov->nc_win_sep = newwin(1,1,0,0);
  return ov;
}

void overlay_clear(Overlay *ov)
{
  wclear(ov->nc_win_st);
  wnoutrefresh(ov->nc_win_st);
}

void overlay_set(Overlay *ov, Buffer *buf)
{
  log_msg("OVERLAY", "overlay_set");
  pos_T size = buf_size(buf);
  pos_T ofs  = buf_ofs(buf);
  size.lnum--;
  buf_set_size(buf, size);
  overlay_clear(ov);
  ov->ov_size = size;
  ov->ov_ofs = (pos_T){ ofs.lnum + size.lnum , ofs.col };
  wresize(ov->nc_win_st, 1, ov->ov_size.col);
  mvwin(ov->nc_win_st, ov->ov_ofs.lnum, ov->ov_ofs.col);
  //mvwin(ov->nc_win_sep, ofs.lnum, ofs.col);
  //wresize(ov->nc_win_sep, size.lnum, 1);
  window_req_draw(ov, overlay_draw);
}

void overlay_draw(void **argv)
{
  Overlay *ov = argv[0];
  wattron(ov->nc_win_st, COLOR_PAIR(4));
  mvwaddstr(ov->nc_win_st, 0, 0, "   FM    ");
  wattroff(ov->nc_win_st, COLOR_PAIR(4));
  wattron(ov->nc_win_st, COLOR_PAIR(3));
  mvwhline (ov->nc_win_st, 0, 9, ' ', ov->ov_size.col);
  //mvwvline (ov->nc_win_sep, 0, ov->ov_size.col-1, ACS_VLINE, 1);
  //mvwaddstr(ov->nc_win_st, 0, 0, "     ");
  wattroff(ov->nc_win_st, COLOR_PAIR(3));
  wnoutrefresh(ov->nc_win_st);
}
