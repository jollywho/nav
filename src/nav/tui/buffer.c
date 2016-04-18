#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/tui/overlay.h"
#include "nav/tui/message.h"
#include "nav/tui/select.h"
#include "nav/event/hook.h"
#include "nav/ascii.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/regex.h"
#include "nav/option.h"
#include "nav/info.h"
#include "nav/event/input.h"
#include "nav/event/fs.h"
#include "nav/util.h"

static void buf_mv_page();
static void buf_mv();
static void buf_search();
static void buf_gen_event();
static void buf_toggle_sel();
static void buf_alt_origin();
static void buf_esc();
static void buf_draw(void **);

/* BUF_GEN_EVENTS */
#define EV_PASTE  0
#define EV_REMOVE 1
#define EV_LEFT   2
#define EV_RIGHT  3
#define EV_JUMP   4

static char *buf_events[] = {
  "paste","remove","left","right","jump",
};

static fn_key key_defaults[] = {
  {ESC,     buf_esc,         0,           0},
  {Ctrl_J,  buf_mv_page,     0,           FORWARD},
  {Ctrl_K,  buf_mv_page,     0,           BACKWARD},
  {'n',     buf_search,      0,           FORWARD},
  {'N',     buf_search,      0,           BACKWARD},
  {'j',     buf_mv,          0,           FORWARD},
  {'k',     buf_mv,          0,           BACKWARD},
  {'g',     oper,            NCH_S,       BACKWARD},
  {'G',     buf_g,           0,           FORWARD},
  {'V',     buf_toggle_sel,  0,           0},
  {'o',     buf_alt_origin,  0,           0},
  {'y',     oper,            NCH,         OP_YANK},
  {'d',     oper,            NCH,         OP_DELETE},
  {'m',     oper,            NCH_A,       OP_MARK},
  {'\'',    oper,            NCH_A,       OP_JUMP},
  {'p',     buf_gen_event,   EV_PASTE,    0},
  {'X',     buf_gen_event,   EV_REMOVE,   0},
  {'h',     buf_gen_event,   EV_LEFT,     0},
  {'l',     buf_gen_event,   EV_RIGHT,    0},
  {Ctrl_O,  buf_gen_event,   EV_JUMP,     BACKWARD},
  {Ctrl_I,  buf_gen_event,   EV_JUMP,     FORWARD},
};

static fn_keytbl key_tbl;
static short cmd_idx[LENGTH(key_defaults)];
#define SZ_LEN 6
#define MAX_POS(x) ((x)-(SZ_LEN+1))
static char szbuf[SZ_LEN*2];

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
  buf->col_focus = opt_color(BUF_SEL_ACTIVE);
  buf->col_text  = opt_color(BUF_TEXT);
  buf->col_dir   = opt_color(BUF_DIR);
  buf->col_sz    = opt_color(BUF_SZ);
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
  buf = NULL;
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
  int newtop = MAX(1 + buf_index(buf) - buf->b_size.lnum, 0);
  int diff = buf->top - newtop;
  buf->top = newtop;
  buf->lnum += diff;
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

void buf_update_progress(Buffer *buf, long percent)
{
  if (!buf)
    return;
  overlay_progress(buf->ov, percent);
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

static void set_focus_col(Buffer *buf, int focus)
{
  if (focus)
    buf->col_focus = opt_color(BUF_SEL_ACTIVE);
  else
    buf->col_focus = opt_color(BUF_SEL_INACTIVE);
}

void buf_toggle_focus(Buffer *buf, int focus)
{
  log_msg("BUFFER", "buf_toggle_focus %d", focus);
  if (!buf)
    return;

  set_focus_col(buf, focus);
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

static void draw_cur_line(Buffer *buf)
{
  attr_t attr = A_NORMAL;
  if (select_has_line(buf, buf->lnum + buf->top))
    attr = A_REVERSE;
  mvwchgat(buf->nc_win, buf->lnum, 0, -1, attr, buf->col_focus, NULL);
}

static void draw_lines(Buffer *buf, Model *m)
{
  for (int i = 0; i < buf->b_size.lnum; ++i) {
    char *it = model_str_line(m, buf->top + i);
    if (!it)
      break;

    fn_rec *rec = model_rec_line(m, buf->top + i);

    readable_fs(rec_stsize(rec), szbuf);

    attr_t attr = A_NORMAL;
    if (select_has_line(buf, buf->top + i))
      attr = A_REVERSE;

    int max = MAX_POS(buf->b_size.col);
    draw_wide(buf->nc_win, i, 0, it, max - 1);

    if (isrecreg(rec)) {
      int col = get_syn_colpair(file_ext(it));
      mvwchgat(buf->nc_win, i, 0, -1, attr, col, NULL);
      draw_wide(buf->nc_win, i, 2+max, szbuf, SZ_LEN);
      mvwchgat(buf->nc_win, i, 2+max, -1, attr, buf->col_sz, NULL);
      mvwchgat(buf->nc_win, i, buf->b_size.col - 1, 1, attr, buf->col_text,0);
    }
    else {
      char *symb = "?";
      if (isreclnk(rec))
        symb = ">";
      else if (isrecdir(rec))
        symb = "/";

      mvwchgat(buf->nc_win, i, 0, -1, attr, buf->col_dir, NULL);
      draw_wide(buf->nc_win, i, buf->b_size.col - 1, symb, SZ_LEN);
      mvwchgat(buf->nc_win, i, buf->b_size.col - 1, 1, attr, buf->col_sz, NULL);
    }

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
  draw_cur_line(buf);
  wnoutrefresh(buf->nc_win);
}

static void buf_curs_move(Buffer *buf, Model *m)
{
  char *curval = model_str_line(m, buf->top + buf->lnum);
  model_set_curs(m, buf->top + buf->lnum);
  select_enter(buf, buf_index(buf));
  buf_refresh(buf);
  send_hook_msg("cursor_change", buf->plugin, NULL, &(HookArg){NULL,curval});
}

void buf_move_invalid(Buffer *buf, int index, int lnum)
{
  Model *m = buf->hndl->model;
  buf->top = index;
  buf->lnum = lnum;
  buf_curs_move(buf, m);
  model_set_curs(m, buf->top + buf->lnum);
}

void buf_full_invalidate(Buffer *buf, int index, int lnum)
{
  log_msg("BUFFER", "buf_full_invalidate");
  if (!buf->attached)
    return;
  select_clear(buf);
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
  int next = regex_next(buf->matches, buf_index(buf), ca->arg);
  int count = regex_match_count(buf->matches);
  char *str = regex_str(buf->matches);
  if (!str)
    nv_err("No search string");
  else if (next == -1)
    nv_err("No matches: %s", str);
  else if (count == 1)
    nv_msg("Single match: %s", str);
  else
    nv_msg("Match %d of %d: %s", next + 1, count, str);
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
    buf_scroll(buf, y, m_max);
  else if (y > 0 && buf->lnum > buf->b_size.lnum * 0.8)
    buf_scroll(buf, y, m_max);

  if (buf->lnum < 0)
    buf->lnum = 0;
  else if (buf->lnum > buf->b_size.lnum)
    buf->lnum = buf->b_size.lnum;

  if (buf->lnum + buf->top > m_max - 1) {
    int count = m_max - buf->top;
    buf->lnum = count - 1;
  }
  buf_curs_move(buf, m);
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
    ret = find_do_op(ca, buf);

  return ret;
}

static void buf_mv_page(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_mv_page");
  int dir = ca->arg;
  int y = (buf->b_size.lnum / 2) * dir;
  buf_move(buf, y, 0);
}

void buf_g(void *_b, Keyarg *ca)
{
  Buffer *buf = (Buffer*)_b;
  int dir = ca->arg;
  int y = model_count(buf->hndl->model) * dir;
  buf_move(buf, y, 0);
}

varg_T buf_focus_sel(Buffer *buf, const char *fld)
{
  log_msg("BUFFER", "buf_focus_sel");
  Model *m = buf->hndl->model;

  if (!select_active()) {
    void *val = model_curs_value(m, fld);
    if (!val)
      val = "";
    char **str = malloc(sizeof(char*));
    str[0] = strdup(val);
    return (varg_T){1,str};
  }

  int count = 0;
  for (int i = 0; i < model_count(m); ++i) {
    if (select_has_line(buf, i))
      count++;
  }

  int j = 0;
  char **str = malloc((1+count)*sizeof(char*));
  for (int i = 0; i < model_count(m); ++i) {
    if (select_has_line(buf, i))
      str[j++] = strdup(model_fld_line(m, fld, i));
  }

  return (varg_T){count,str};
}

void buf_end_sel(Buffer *buf)
{
  select_min_origin(buf, &buf->lnum, &buf->top);
  select_clear(buf);
  buf_move_invalid(buf, buf->top, buf->lnum);
  buf_refresh(buf);
}

void buf_yank(void *_b, Keyarg *ca)
{
  Buffer *buf = (Buffer*)_b;
  fn_handle *h = buf->hndl;

  char *field = h->fname;
  if (ca->key == 'f')
    field = "fullpath";
  else if (ca->key == 'd')
    field = "dir";
  else if (ca->key != 'y')
    return;

  reg_set(NUL, buf_focus_sel(buf, "fullpath"));
  reg_set('0', buf_focus_sel(buf, field));
  buf_end_sel(buf);
}

void buf_del(void *_b, Keyarg *ca)
{
  Buffer *buf = (Buffer*)_b;
  fn_handle *h = buf->hndl;
  reg_set(NUL, buf_focus_sel(buf, "fullpath"));
  reg_set('1', buf_focus_sel(buf, h->fname));
  buf_end_sel(buf);
}

void buf_mark(void *_b, Keyarg *ca)
{
  log_msg("BUFFER", "buf_mark");
  Buffer *buf = (Buffer*)_b;
  mark_chr_str(ca->key, buf->hndl->key);
}

void buf_gomark(void *_b, Keyarg *ca)
{
  log_msg("BUFFER", "buf_gomark");
  Buffer *buf = (Buffer*)_b;
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
  if (ca->type > LENGTH(buf_events) || ca->type < 0)
    return;
  send_hook_msg(buf_events[ca->type], buf->plugin, NULL, &(HookArg){ca,NULL});
}

static void buf_toggle_sel(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_toggle_sel");
  select_toggle(buf, model_count(buf->hndl->model));
  buf_refresh(buf);
}

static void buf_alt_origin(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_alt_origin");
  if (select_alt_origin(buf, &buf->lnum, &buf->top))
    buf_move_invalid(buf, buf->top, buf->lnum);
}

static void buf_esc(Buffer *buf, Keyarg *ca)
{
  log_msg("BUFFER", "buf_esc");
  buf_end_sel(buf);
}

void buf_sort(Buffer *buf, char *fld, int flags)
{
  if (!buf->hndl)
    return;
  DO_EVENTS_UNTIL(!model_blocking(buf->hndl));
  int type = fld_type(buf->hndl->tn, fld);
  sort_t sort = {type,flags};
  model_sort(buf->hndl->model, sort);
}

/* public fields */
int buf_index(Buffer *buf)
{return buf->lnum+buf->top;}
int buf_line(Buffer *buf)
{return buf->lnum;}
int buf_top(Buffer *buf)
{return buf->top;}
int buf_id(Buffer *buf)
{return buf->plugin ? buf->plugin->id : 0;}
int buf_sel_count(Buffer *buf)
{return select_count();}
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
