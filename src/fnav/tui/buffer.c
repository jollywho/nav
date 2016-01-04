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

typedef struct {
  String name;
  enum Type type;
  void (*f)();
} Key;

static void buf_mv_page();
static void buf_mv();
static void buf_search();
static void buf_g();

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
};
static fn_keytbl key_tbl;
static short cmd_idx[KEYS_SIZE];

#define SET_POS(pos,y,x)       \
  (pos.col) = (x);             \
  (pos.lnum) = (y);            \

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
  input_init_tbl(&key_tbl);
}

Buffer* buf_new()
{
  log_msg("BUFFER", "new");
  Buffer *buf = malloc(sizeof(Buffer));
  SET_POS(buf->b_size, 0, 0);
  SET_POS(buf->b_ofs, 0, 0);
  buf->nc_win = newwin(1,1,0,0);
  buf->dirty = false;
  buf->queued = false;
  buf->attached = false;
  buf->closed = false;
  buf->focused = false;
  buf->input_cb = NULL;
  buf->matches = NULL;
  buf->lnum = buf->top = 0;
  buf->ldif = 0;
  buf->col_select = attr_color("BufSelected");
  buf->col_text = attr_color("BufText");
  buf->col_dir = attr_color("BufDir");
  return buf;
}

void buf_delete(Buffer *buf)
{
  log_msg("BUFFER", "cleanup");
  delwin(buf->nc_win);
  free(buf);
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
  buf->closed = true;
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

void buf_set_linematch(Buffer *buf, LineMatch *match)
{
  log_msg("BUFFER", "buf_set_linematch");
  regex_destroy(buf);
  buf->matches = match;
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
  werase(buf->nc_win);
  if (buf->closed) {
    wnoutrefresh(buf->nc_win);
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
  buf->dirty = false;
  buf->queued = false;
  buf->ldif = 0;
}

void buf_full_invalidate(Buffer *buf, int index, int lnum)
{
  // buffer reset and reentrance
  log_msg("BUFFER", "buf_full_invalidate");
  Model *m = buf->hndl->model;
  werase(buf->nc_win);
  buf->top = index;
  buf->lnum = lnum;
  regex_destroy(buf);
  model_set_curs(m, buf->top + buf->lnum);
  buf_refresh(buf);
}

// input entry point.
// proper commands are chosen based on buffer state.
int buf_input(Buffer *buf, int key)
{
  log_msg("BUFFER", "input");
  if (buf->input_cb) {
    buf->input_cb(buf, key);
    return 1;
  }
  if (!buf->attached)
    return 0;
  if (model_blocking(buf->hndl))
    return 0;
  if (buf->cntlr) {
    if (buf->cntlr->_input(buf->cntlr, key))
      return 1;
  }
  // TODO: check consumed
  Cmdarg ca;
  int idx = find_command(&key_tbl, key);
  ca.arg = key_defaults[idx].cmd_arg;
  if (idx >= 0) {
    key_defaults[idx].cmd_func(buf, &ca);
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
    regex_next(buf->top + buf->lnum);
  if (arg->arg == BACKWARD)
    regex_prev(buf->top + buf->lnum);
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

void buf_sort(Buffer *buf, String fld, int flags)
{
  if (!buf->hndl) return;
  model_sort(buf->hndl->model, fld, flags);
}
