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
#include "fnav/info.h"
#include "fnav/event/input.h"
#include "fnav/event/fs.h"

static void buf_oper();
static void buf_mv_page();
static void buf_mv();
static void buf_search();
static void buf_g();
static void buf_mark();
static void buf_gomark();
static void buf_yank();
static void buf_del();
static void buf_gen_event();
static void buf_draw(void **);

#define EV_PASTE  0
#define EV_REMOVE 1
#define EV_LEFT   2
#define EV_RIGHT  3

static char *buf_events[] = {
  "paste","remove","left","right",
};

static fn_oper key_operators[] = {
  {NUL,      NUL,    NULL},
  {'y',      NUL,    buf_yank},
  {'m',      NUL,    buf_mark},
  {'\'',     NUL,    buf_gomark},
  {'g',      NUL,    buf_g},
  {'d',      NUL,    buf_del},
};

static fn_key key_defaults[] = {
  {Ctrl_J,  buf_mv_page,     0,           FORWARD},
  {Ctrl_K,  buf_mv_page,     0,           BACKWARD},
  {'n',     buf_search,      0,           FORWARD},
  {'N',     buf_search,      0,           BACKWARD},
  {'j',     buf_mv,          0,           FORWARD},
  {'k',     buf_mv,          0,           BACKWARD},
  {'g',     buf_oper,        NCH_S,       BACKWARD},
  {'G',     buf_g,           0,           FORWARD},
  {'y',     buf_oper,        NCH,         OP_YANK},
  {'d',     buf_oper,        NCH,         OP_DELETE},
  {'m',     buf_oper,        NCH_A,       OP_MARK},
  {'\'',    buf_oper,        NCH_A,       OP_JUMP},
  {'p',     buf_gen_event,   0,           EV_PASTE},
  {'X',     buf_gen_event,   0,           EV_REMOVE},
  {'h',     buf_gen_event,   0,           EV_LEFT},
  {'l',     buf_gen_event,   0,           EV_RIGHT},
};

static fn_keytbl key_tbl;
static short cmd_idx[LENGTH(key_defaults)];

void buf_init()
{
  log_msg("BUFFER", "init");
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = LENGTH(key_defaults);
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
  if (!buf->del)
    return 0;

  werase(buf->nc_win);
  wnoutrefresh(buf->nc_win);
  delwin(buf->nc_win);
  free(buf);
  return 1;
}

void buf_delete(Buffer *buf)
{
  log_msg("BUFFER", "delete");
  buf->del = true;
  if (!buf->queued)
    buf_expire(buf);
}

void buf_detach(Buffer *buf)
{
  log_msg("BUFFER", "buf_detach");
  buf->attached = false;
  buf->plugin = NULL;
  overlay_erase(buf->ov);
}

static void resize_adjustment(Buffer *buf)
{
  log_msg("BUFFER", "resize_adjustment");
  log_msg("BUFFER", "%d %d", buf->top, buf->ldif);
  int diff = buf->top + buf->ldif;
  if (diff < 0)
    diff = 0;

  int line = (buf->top + buf->lnum) - diff;
  buf->top = diff;
  buf->lnum = line;
}

void buf_set_size_ofs(Buffer *buf, pos_T size, pos_T ofs)
{
  log_msg("BUFFER", "SET SIZE %d %d", size.lnum, size.col);

  size.lnum--;
  if (buf->b_size.lnum > 0)
    buf->ldif = buf->b_size.lnum - size.lnum;

  int sep = 0;
  if (ofs.col != 0) {
    sep = 1;
    ofs.col++;
    size.col--;
  }

  buf->b_size = size;
  buf->b_ofs = ofs;

  delwin(buf->nc_win);
  buf->nc_win = newwin(size.lnum, size.col, ofs.lnum, ofs.col);

  overlay_set(buf->ov, buf->b_size, buf->b_ofs, sep);

  int m_max = 0;
  if (buf->attached)
    m_max = model_count(buf->hndl->model);

  int needs_to_grow = buf->top + buf->b_size.lnum > m_max;
  int out_of_bounds = buf->lnum >= buf->b_size.lnum;
  if ((out_of_bounds) || (buf->ldif < 0 && needs_to_grow))
    resize_adjustment(buf);

  buf_refresh(buf);
}

void buf_set_pass(Buffer *buf)
{
  buf->nodraw = true;
  buf->attached = false;
}

void buf_set_plugin(Buffer *buf, Plugin *plugin)
{
  log_msg("BUFFER", "buf_set_plugin");
  buf->plugin = plugin;
  buf->hndl = plugin->hndl;
  buf->attached = true;
  buf->nodraw = false;
  overlay_bufno(buf->ov, plugin->id);
  overlay_edit(buf->ov, plugin->fmt_name, 0, 0);
}

void buf_set_status(Buffer *buf, char *name, char *usr, char *in)
{
  overlay_edit(buf->ov, name, usr, in);
}

void buf_toggle_focus(Buffer *buf, int focus)
{
  log_msg("BUFFER", "buf_toggle_focus %d", focus);
  if (!buf)
    return;

  buf->focused = focus;
  if (buf->plugin && buf->plugin->_focus && focus)
    buf->plugin->_focus(buf->plugin);

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

static void draw_cur_line(Buffer *buf, Model *m)
{
  char *it = model_str_line(m, buf->top + buf->lnum);
  TOGGLE_ATTR(!buf->focused, buf->nc_win, A_REVERSE);
  DRAW_STR(buf, nc_win, buf->lnum, 0, it, col_select);
  wattroff(buf->nc_win, A_REVERSE);
}

static void draw_lines(Buffer *buf, Model *m)
{
  for (int i = 0; i < buf->b_size.lnum; ++i) {
    char *it = model_str_line(m, buf->top + i);
    if (!it)
      break;

    // TODO: plugin CB to filter string
    char *path = model_fld_line(m, "fullpath", buf->top + i);
    if (isdir(path))
      DRAW_STR(buf, nc_win, i, 0, it, col_dir);
    else
      DRAW_STR(buf, nc_win, i, 0, it, col_text);
  }
}

void buf_draw(void **argv)
{
  log_msg("BUFFER", "draw");
  Buffer *buf = (Buffer*)argv[0];
  if (buf_expire(buf))
    return;

  buf->dirty = false;
  buf->queued = false;
  buf->ldif = 0;
  werase(buf->nc_win);

  if (buf->nodraw || !buf->attached) {
    wnoutrefresh(buf->nc_win);
    send_hook_msg("window_resize", buf->plugin, NULL, NULL);
    return;
  }
  overlay_lnum(buf->ov, buf_index(buf), model_count(buf->hndl->model));

  Model *m = buf->hndl->model;
  draw_lines(buf, m);
  draw_cur_line(buf, m);

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
  log_msg("BUFFER", "buf_full_invalidate");
  if (!buf->attached)
    return;
  regex_del_matches(buf->matches);
  buf_move_invalid(buf, index, lnum);
}

void buf_scroll(Buffer *buf, int y, int max)
{
  log_msg("BUFFER", "scroll %d %d", buf->lnum, y);
  int prev = buf->top;
  buf->top += y;

  if (buf->top + buf->b_size.lnum > max)
    buf->top = max - buf->b_size.lnum;
  if (buf->top < 0)
    buf->top = 0;

  int diff = prev - buf->top;
  buf->lnum += diff;
}

static void buf_search(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_search");
  if (ca->arg == FORWARD)
    regex_next(buf->matches, buf->top + buf->lnum);
  if (ca->arg == BACKWARD)
    regex_prev(buf->matches, buf->top + buf->lnum);
}

void buf_move(Buffer *buf, int y, int x)
{
  log_msg("BUFFER", "buf_move");
  Keyarg arg = {.arg = y};
  buf_mv(buf, &arg);
}

static void buf_mv(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_mv %d", ca->arg);
  /* move cursor N times in a dimension;
   * scroll if cursor is on an edge. */
  Model *m = buf->hndl->model;
  int y = ca->arg;
  int m_max = model_count(buf->hndl->model);
  buf->lnum += y;

  if (y < 0 && buf->lnum < buf->b_size.lnum * 0.2)
    buf_scroll(buf, ca->arg, m_max);
  else if (y > 0 && buf->lnum > buf->b_size.lnum * 0.8)
    buf_scroll(buf, ca->arg, m_max);

  if (buf->lnum < 0)
    buf->lnum = 0;
  else if (buf->lnum > buf->b_size.lnum)
    buf->lnum = buf->b_size.lnum;

  if (buf->lnum + buf->top > m_max - 1) {
    int count = m_max - buf->top;
    buf->lnum = count - 1;
  }
  model_set_curs(m, buf->top + buf->lnum);
  char *curval = model_str_line(m, buf->top + buf->lnum);
  send_hook_msg("cursor_change", buf->plugin, NULL, &(HookArg){NULL,curval});
  buf_refresh(buf);
}

int buf_input(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "input");
  if (!buf->attached)
    return 0;
  if (model_blocking(buf->hndl))
    return 0;

  if (!buf->plugin)
    return 0;

  int ret = find_do_cmd(&key_tbl, ca, buf);
  if (!ret)
    ret = find_do_op(key_operators, ca, buf);

  return ret;
}

static void buf_oper(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_oper");
  clearop(ca);
  if (ca->type == NCH_S)
    ca->oap.key = OP_G;
  else
    ca->oap.key = ca->arg;

  ca->nkey = ca->key;
}

static void buf_mv_page(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_mv_page");
  int dir = ca->arg;
  int y = (buf->b_size.lnum / 2) * dir;
  buf_move(buf, y, 0);
}

static void buf_g(Buffer *buf, Keyarg *ca)
{
  int dir = ca->arg;
  int y = model_count(buf->hndl->model) * dir;
  buf_move(buf, y, 0);
}

static void buf_yank(Buffer *buf, Keyarg *ca)
{
  char *val = model_curs_value(buf->hndl->model, "fullpath");
  reg_set(NUL, val);
  reg_set('0', val);
}

static void buf_del(Buffer *buf, Keyarg *ca)
{
  char *val = model_curs_value(buf->hndl->model, "fullpath");
  reg_set(NUL, val);
  reg_set('1', val);
}

static void buf_mark(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_mark");
  mark_chr_str(ca->key, buf->hndl->key);
}

static void buf_gomark(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_gomark");
  char *path = mark_str(ca->key);
  if (!path)
    return;
  path = strdup(path);
  mark_chr_str('\'', buf->hndl->key);
  send_hook_msg("open", buf->plugin, NULL, &(HookArg){NULL,path});
  free(path);
}

static void buf_gen_event(Buffer *buf, Keyarg *ca)
{
  if (ca->arg > LENGTH(buf_events) || ca->arg < 0)
    return;
  send_hook_msg(buf_events[ca->arg], buf->plugin, NULL, NULL);
}

void buf_sort(Buffer *buf, char *fld, int flags)
{
  if (!buf->hndl)
    return;
  do_events_until(model_blocking, buf->hndl);
  model_sort(buf->hndl->model, fld, flags);
}

/* public fields */
int buf_index(Buffer *buf)
{return buf->lnum+buf->top;}
int buf_line(Buffer *buf)
{return buf->lnum;}
int buf_top(Buffer *buf)
{return buf->top;}
pos_T buf_size(Buffer *buf)
{return buf->b_size;}
pos_T buf_ofs(Buffer *buf)
{return buf->b_ofs;}
Plugin* buf_plugin(Buffer *buf)
{return buf->plugin;}
pos_T buf_pos(Buffer *buf)
{return (pos_T){buf->lnum+1,0};}
int buf_attached(Buffer *buf)
{return buf->attached;}
void buf_set_overlay(Buffer *buf, Overlay *ov)
{buf->ov = ov;}
WINDOW* buf_ncwin(Buffer *buf)
{return buf->nc_win;}
