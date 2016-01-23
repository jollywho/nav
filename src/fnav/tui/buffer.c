#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"
#include "fnav/tui/overlay.h"
#include "fnav/event/hook.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/regex.h"
#include "fnav/option.h"
#include "fnav/event/input.h"
#include "fnav/event/fs.h"

enum Type { OPERATOR, MOTION };

static void buf_mv_page();
static void buf_mv();
static void buf_search();
static void buf_g();
static void buf_yank();
static void buf_gen_event();

#define EV_PASTE  0
#define EV_REMOVE 1
#define MAX_EVENTS ARRAY_SIZE(buf_events)
static String buf_events[] = {"paste", "remove"};

#define KEYS_SIZE ARRAY_SIZE(key_defaults)
static fn_key key_defaults[] = {
  {Ctrl_J,  buf_mv_page,     0,           FORWARD},
  {Ctrl_K,  buf_mv_page,     0,           BACKWARD},
  {'n',     buf_search,      0,           FORWARD},
  {'N',     buf_search,      0,           BACKWARD},
  {'j',     buf_mv,          0,           FORWARD},
  {'k',     buf_mv,          0,           BACKWARD},
  {'g',     buf_g,           0,           BACKWARD},
  {'G',     buf_g,           0,           FORWARD},
  {'y',     buf_yank,        0,           0},
  {'p',     buf_gen_event,   0,           EV_PASTE},
  {'X',     buf_gen_event,   0,           EV_REMOVE},
};
static fn_keytbl key_tbl;
static short cmd_idx[KEYS_SIZE];

#define INC_POS(pos,x,y)       \
  (pos.col) += (x);            \
  (pos.lnum) += (y);           \

void buf_listen(fn_handle *hndl);
void buf_draw(void **argv);

void buf_init()
{
  log_msg("BUFFER", "init");
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = KEYS_SIZE;
  input_setup_tbl(&key_tbl);
}

void buf_cleanup()
{
}

Buffer* buf_new()
{
  log_msg("BUFFER", "new");
  Buffer *buf = malloc(sizeof(Buffer));
  memset(buf, 0, sizeof(Buffer));
  buf->nc_win = newwin(1,1,0,0);
  buf->col_select = attr_color("BufSelected");
  buf->col_text = attr_color("BufText");
  buf->col_dir = attr_color("BufDir");
  return buf;
}

static int buf_expire(Buffer *buf)
{
  if (buf->del) {
    werase(buf->nc_win);
    wnoutrefresh(buf->nc_win);
    delwin(buf->nc_win);
    free(buf);
    return 1;
  }
  return 0;
}

void buf_delete(Buffer *buf)
{
  log_msg("BUFFER", "delete");
  buf->del = true;
  if (!buf->queued)
    buf_expire(buf);
}

static void resize_adjustment(Buffer *buf)
{
  log_msg("BUFFER", "resize_adjustment");
  log_msg("BUFFER", "%d %d", buf->top, buf->ldif);
  int diff = buf->top + buf->ldif;
  if (diff < 0) diff = 0;
  int line = (buf->top + buf->lnum) - diff;
  buf->top = diff;
  buf->lnum = line;
}

void buf_set_size_ofs(Buffer *buf, pos_T size, pos_T ofs)
{
  log_msg("BUFFER", "SET SIZE %d %d", size.lnum, size.col);
  size.lnum--;
  if (buf->b_size.lnum > 0) {
    buf->ldif = buf->b_size.lnum - size.lnum;
  }

  int sep;
  if (ofs.col == 0) {
    sep = 0;
  }
  else {
    sep = 1;
    ofs.col++;
    size.col--;
  }
  buf->b_size = size;
  buf->b_ofs = ofs;

  delwin(buf->nc_win);
  buf->nc_win = newwin(buf->b_size.lnum, buf->b_size.col,
                       buf->b_ofs.lnum,  buf->b_ofs.col);

  overlay_set(buf->ov, buf->b_size, buf->b_ofs, sep);

  if ((buf->ldif > 0 && buf->lnum >= buf->b_size.lnum) || (buf->ldif < 0)) {
    resize_adjustment(buf);
  }
  buf_refresh(buf);
}

void buf_set_pass(Buffer *buf)
{
  buf->nodraw = true;
  buf->attached = false;
}

void buf_set_cntlr(Buffer *buf, Cntlr *cntlr)
{
  log_msg("BUFFER", "buf_set_cntlr");
  buf->cntlr = cntlr;
  buf->hndl = cntlr->hndl;
  buf->attached = true;
  overlay_edit(buf->ov, cntlr->fmt_name, 0, 0, 0);
}

void buf_set_status(Buffer *buf, String name, String usr, String in, String out)
{
  overlay_edit(buf->ov, name, usr, in, out);
}

void buf_toggle_focus(Buffer *buf, int focus)
{
  log_msg("BUFFER", "buf_toggle_focus %d", focus);
  if (!buf) return;
  buf->focused = focus;
  if (buf->attached)
    buf->cntlr->_focus(buf->cntlr);
  buf_refresh(buf);
}

void buf_refresh(Buffer *buf)
{
  log_msg("BUFFER", "refresh");
  if (buf->queued)
    return;
  buf->queued = true;
  window_req_draw(buf, buf_draw);
}

void buf_draw(void **argv)
{
  log_msg("BUFFER", "draw");
  Buffer *buf = (Buffer*)argv[0];
  if (buf_expire(buf)) return;

  werase(buf->nc_win);

  buf->dirty = false;
  buf->queued = false;
  buf->ldif = 0;

  if (buf->nodraw) {
    wnoutrefresh(buf->nc_win);
    send_hook_msg("window_resize", buf->cntlr, NULL);
    return;
  }
  if (buf->attached) {
    Model *m = buf->hndl->model;
    for (int i = 0; i < buf->b_size.lnum; ++i) {
      String it = model_str_line(m, buf->top + i);
      if (!it) break;
      String path = model_fld_line(m, "fullpath", buf->top + i);
      if (isdir(path))
        DRAW_STR(buf, nc_win, i, 0, it, col_dir);
      else
        DRAW_STR(buf, nc_win, i, 0, it, col_text);
    }
    String it = model_str_line(m, buf->top + buf->lnum);

    TOGGLE_ATTR(!buf->focused, buf->nc_win, A_REVERSE);
    DRAW_STR(buf, nc_win, buf->lnum, 0, it, col_select);
    wattroff(buf->nc_win, A_REVERSE);
  }
  wnoutrefresh(buf->nc_win);
}

void buf_move_invalid(Buffer *buf, int index, int lnum)
{
  Model *m = buf->hndl->model;
  buf->top = index;
  buf->lnum = lnum;
  model_set_curs(m, buf->top + buf->lnum);
  buf_refresh(buf);
}

void buf_full_invalidate(Buffer *buf, int index, int lnum)
{
  // buffer reset and reentrance
  log_msg("BUFFER", "buf_full_invalidate");
  if (!buf->attached) return;
  regex_del_matches(buf->matches);
  buf_move_invalid(buf, index, lnum);
}

// input entry point.
// proper commands are chosen based on buffer state.
int buf_input(Buffer *buf, Cmdarg *ca)
{
  log_msg("BUFFER", "input");
  if (buf->input_cb) {
    buf->input_cb(buf, ca);
    return 1;
  }
  if (!buf->attached)
    return 0;
  if (model_blocking(buf->hndl))
    return 0;
  if (buf->cntlr) {
    if (buf->cntlr->_input(buf->cntlr, ca))
      return 1;
  }
  // TODO: check consumed
  int idx = find_command(&key_tbl, ca->key);
  ca->arg = key_defaults[idx].cmd_arg;
  if (idx >= 0) {
    key_defaults[idx].cmd_func(buf, ca);
  }
  return 0;
}

void buf_scroll(Buffer *buf, int y, int max)
{
  log_msg("BUFFER", "scroll %d %d", buf->lnum, y);
  int prev = buf->top;
  buf->top += y;
  if (buf->top + buf->b_size.lnum > max) {
    buf->top = max - buf->b_size.lnum;
  }
  if (buf->top < 0) {
    buf->top = 0;
  }
  int diff = prev - buf->top;
  buf->lnum += diff;
}

static void buf_search(Buffer *buf, Cmdarg *arg)
{
  log_msg("BUFFER", "buf_search");
  if (arg->arg == FORWARD)
    regex_next(buf->matches, buf->top + buf->lnum);
  if (arg->arg == BACKWARD)
    regex_prev(buf->matches, buf->top + buf->lnum);
}

void buf_move(Buffer *buf, int y, int x)
{
  log_msg("BUFFER", "buf_move");
  Cmdarg arg = {.arg = y};
  buf_mv(buf, &arg);
}

static void buf_mv(Buffer *buf, Cmdarg *arg)
{
  log_msg("BUFFER", "buf_mv %d", arg->arg);
  // move cursor N times in a dimension.
  // scroll if located on an edge.
  Model *m = buf->hndl->model;
  int y = arg->arg;
  int m_max = model_count(buf->hndl->model);
  buf->lnum += y;

  if (y < 0 && buf->lnum < buf->b_size.lnum * 0.2) {
    buf_scroll(buf, arg->arg, m_max);
  }
  else if (y > 0 && buf->lnum > buf->b_size.lnum * 0.8) {
    buf_scroll(buf, arg->arg, m_max);
  }

  if (buf->lnum < 0) {
    buf->lnum = 0;
  }
  else if (buf->lnum > buf->b_size.lnum) {
    buf->lnum = buf->b_size.lnum;
  }
  if (buf->lnum + buf->top > m_max - 1) {
    int count = m_max - buf->top;
    buf->lnum = count - 1;
  }
  model_set_curs(m, buf->top + buf->lnum);
  send_hook_msg("cursor_change", buf->cntlr, NULL);
  buf_refresh(buf);
}

int buf_line(Buffer *buf)
{return buf->lnum;}
int buf_top(Buffer *buf)
{return buf->top;}
pos_T buf_size(Buffer *buf)
{return buf->b_size;}
pos_T buf_ofs(Buffer *buf)
{return buf->b_ofs;}
Cntlr* buf_cntlr(Buffer *buf)
{return buf->cntlr;}
pos_T buf_pos(Buffer *buf)
{return (pos_T){buf->lnum+1,0};}
int buf_attached(Buffer *buf)
{return buf->attached;}
void buf_set_overlay(Buffer *buf, Overlay *ov)
{buf->ov = ov;}

static void buf_mv_page(Buffer *buf, Cmdarg *arg)
{
  log_msg("BUFFER", "buf_mv_page");
  int dir = arg->arg;
  int y = (buf->b_size.lnum / 2) * dir;
  buf_move(buf, y, 0);
}

static void buf_g(Buffer *buf, Cmdarg *arg)
{
  int dir = arg->arg;
  int y = model_count(buf->hndl->model) * dir;
  buf_move(buf, y, 0);
}

static void buf_yank(Buffer *buf, Cmdarg *arg)
{
  reg_set(buf->hndl, "0", "fullpath");
}

static void buf_gen_event(Buffer *buf, Cmdarg *arg)
{
  if (arg->arg > MAX_EVENTS || arg->arg < 0) return;
  send_hook_msg(buf_events[arg->arg], buf->cntlr, NULL);
}

void buf_sort(Buffer *buf, String fld, int flags)
{
  if (!buf->hndl) return;
  model_sort(buf->hndl->model, fld, flags);
}
