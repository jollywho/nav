// model
// stream and table callbacks should be directed here for
// managing data structures read by attached buffers.
#include <malloc.h>
#include <sys/time.h>
#include <uv.h>

#include "nav/lib/utarray.h"

#include "nav/model.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/nav.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/event/fs.h"
#include "nav/option.h"

struct fn_line {
  fn_rec *rec;
};
static const UT_icd icd = {sizeof(fn_line),NULL,NULL,NULL};

struct Model {
  fn_handle *hndl;  //opened handle
  fn_lis *lis;      //listener
  fn_rec *cur;      //current rec
  ventry *head;     //head entry of listener
  bool blocking;
  char *pfval;      //prev field value
  int plnum;        //prev lnum
  int ptop;         //prev top
  sort_t sort;
  UT_array *lines;
};
static sort_t focus = {0,1};

typedef struct {
  int i;
  bool rev;
} sort_ent;

static int cmp_str(const void *, const void *, void *);
static int cmp_time(const void *, const void *, void *);
static int cmp_dir(const void *, const void *, void *);
static int cmp_num(const void *, const void *, void *);
static int cmp_type(const void *, const void *, void *);
typedef struct Sort_T Sort_T;
static struct Sort_T {
  int val;
  int (*cmp)(const void *, const void *, void *);
} sort_tbl[] = {
  {SRT_STR,    cmp_str},
  {SRT_TIME,   cmp_time},
  {SRT_DIR,    cmp_dir},
  {SRT_NUM,    cmp_num},
  {SRT_TYPE,   cmp_type},
};
//TODO: remove field name from cmp functions:
//field name can be passed into each cmp fn within an arg struct.

static int cmp_time(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  time_t t1 = rec_ctime(l1.rec);
  time_t t2 = rec_ctime(l2.rec);
  return difftime(t1, t2);
}

static int cmp_str(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  char *s1 = rec_fld(l1.rec, "name");
  char *s2 = rec_fld(l2.rec, "name");
  return strcmp(s2, s1);
}

static int cmp_dir(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  int b1 = rec_stmode(l1.rec);
  int b2 = rec_stmode(l2.rec);
  if (b1 < b2)
    return -1;
  if (b1 > b2)
    return 1;
  return 0;
}

static int cmp_num(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  off_t t1 = rec_stsize(l1.rec);
  off_t t2 = rec_stsize(l2.rec);
  if (t1 < t2)
    return -1;
  if (t1 > t2)
    return 1;
  return 0;
}

static int cmp_type(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  char *s1 = rec_fld(l1.rec, "name");
  char *s2 = rec_fld(l2.rec, "name");

  fn_syn *sy1 = get_syn(file_ext(s1));
  fn_syn *sy2 = get_syn(file_ext(s2));
  if (isrecdir(l1.rec))
    s1 = "";
  else if (sy1)
    s1 = sy1->key;
  if (isrecdir(l2.rec))
    s2 = "";
  else if (sy2)
    s2 = sy2->key;

  return strcmp(s2, s1);
}

static int sort_by_type(const void *a, const void *b, void *arg)
{
  sort_ent *srt = (sort_ent*)arg;
  int ret = sort_tbl[srt->i].cmp(a,b,0);
  if (ret == 0)
    return 0;
  return srt->rev ? ret : -ret;
}

static void do_sort(Model *m)
{
  sort_ent srt = {0, m->sort.sort_rev};
  for (int i = 0; i < LENGTH(sort_tbl); i++) {
    int row_type = sort_tbl[i].val;
    if ((m->sort.sort_type & row_type) == row_type) {
      srt.i = i;
      utarray_sort(m->lines, sort_by_type, &srt);
    }
  }
}

static fn_line* find_by_type(Model *m, fn_line *ln)
{
  sort_ent srt = {0, m->sort.sort_rev};

  for (int i = 0; i < LENGTH(sort_tbl); i++) {
    int row_type = sort_tbl[i].val;
    if ((m->sort.sort_type & row_type) == row_type) {
      srt.i = i;
      break;
    }
  }
  return utarray_find(m->lines, ln, sort_by_type, &srt);
}

static fn_line* find_linear(Model *m, const char *val)
{
  for (int i = 0; i < utarray_len(m->lines); i++) {
    fn_line *ln = (fn_line*)utarray_eltptr(m->lines, i);
    if (!strcmp(val, rec_fld(ln->rec, m->hndl->kname)))
      return ln;
  }
  return NULL;
}

void model_init(fn_handle *hndl)
{
  Model *m = malloc(sizeof(Model));
  memset(m, 0, sizeof(Model));
  m->hndl = hndl;
  hndl->model = m;

  m->sort = (sort_t){0,1};
  m->blocking = true;
  utarray_new(m->lines, &icd);
  Buffer *buf = hndl->buf;
  buf->matches = regex_new(hndl);
  buf->filter = filter_new(hndl);
}

void model_cleanup(fn_handle *hndl)
{
  Model *m = hndl->model;
  regex_destroy(hndl);
  filter_destroy(hndl);
  utarray_free(m->lines);
  if (m->pfval)
    free(m->pfval);
  free(m);
}

void model_inherit(fn_handle *hndl)
{
  hndl->model->sort = focus;
}

void model_open(fn_handle *hndl)
{
  tbl_add_lis(hndl->tn, hndl->key_fld, hndl->key);
}

static void model_save(Model *m)
{
  if (!m->cur)
    return;
  Buffer *b = m->hndl->buf;
  char *curval = model_curs_value(m, m->hndl->kname);
  lis_save(m->lis, buf_top(b), buf_line(b), curval);
  free(m->pfval);
  m->pfval = NULL;
}

void model_close(fn_handle *hndl)
{
  log_msg("MODEL", "model_close");
  Model *m = hndl->model;
  model_save(m);
  m->blocking = true;
  utarray_clear(m->lines);
}

bool model_blocking(fn_handle *hndl)
{
  return hndl->model->blocking;
}

static void refit(Model *m, Buffer *buf)
{
  int bsize = buf_size(buf).lnum;

  /* content above index */
  if (model_count(m) - m->ptop < MIN(model_count(m), bsize)) {
    int prev = m->ptop;
    m->ptop = model_count(m) - bsize;
    m->plnum += (prev - m->ptop);
  }

  /* content below window */
  if (m->plnum > bsize)
    m->plnum = bsize - 1;

  /* content fits without offset */
  if (bsize > model_count(m)) {
    int dif = m->ptop;
    m->ptop = 0;
    m->plnum += dif;
  }

  /* lnum + offset exceeds maximum index */
  if (m->ptop + m->plnum > model_count(m) - 1)
    m->plnum = (model_count(m) - m->ptop) - 1;
}

static int try_old_pos(Model *m, fn_lis *lis, int pos)
{
  fn_line *ln;
  ln = (fn_line*)utarray_eltptr(m->lines, pos);
  if (!ln)
    return 0;
  return (!strcmp(m->pfval, rec_fld(ln->rec, m->hndl->kname)));
}

static void try_old_val(Model *m, fn_lis *lis, ventry *it)
{
  int pos = m->ptop + m->plnum;
  fn_line *find;
  it = ent_rec(it->rec, "dir");
  if (!it)
    return;
  fn_line ln = { .rec = it->rec };

  find = find_by_type(m, &ln);
  if (!find)
    return;

  /* if found value is not stored value */
  if (strcmp(m->pfval, rec_fld(find->rec, m->hndl->kname)))
    find = find_linear(m, m->pfval);

  if (!find)
    return;

  int foundpos = utarray_eltidx(m->lines, find);
  m->ptop = MAX(0, m->ptop + (foundpos - pos));
  m->plnum = MAX(0, foundpos - m->ptop);
}

void refind_line(Model *m)
{
  log_msg("MODEL", "refind_line");
  fn_handle *h = m->hndl;
  fn_lis *lis = m->lis;

  ventry *it = fnd_val(h->tn, m->hndl->kname, m->pfval);
  if (!it)
    return;

  if (try_old_pos(m, lis, m->ptop + m->plnum))
    return;

  try_old_val(m, lis, it);
}

static void model_set_prev(Model *m)
{
  Buffer *buf = m->hndl->buf;
  char *curval = model_curs_value(m, m->hndl->kname);
  if (curval)
    SWAP_ALLOC_PTR(m->pfval, strdup(curval));
  m->ptop = buf_top(buf);
  m->plnum = buf_line(buf);
}

void model_ch_focus(fn_handle *hndl)
{
  if (hndl && hndl->model)
    focus = hndl->model->sort;
}

void model_sort(Model *m, sort_t srt)
{
  log_msg("MODEL", "model_sort");
  if (!m->blocking)
    model_set_prev(m);

  if (srt.sort_type != -1)
    focus = m->sort = srt;

  do_sort(m);
  refind_line(m);
  refit(m, m->hndl->buf);
  buf_full_invalidate(m->hndl->buf, m->ptop, m->plnum);
}

void model_flush(fn_handle *hndl, bool reopen)
{
  log_msg("MODEL", "model_flush");
  Model *m = hndl->model;
  free(m->pfval);
  m->pfval = NULL;

  if (!reopen)
    return;

  model_set_prev(m);
  utarray_clear(m->lines);
  m->blocking = true;
}

void model_recv(Model *m)
{
  log_msg("MODEL", "model_recv");
  log_msg("MODEL", "block? %d", m->blocking);
  if (!m->blocking)
    return;

  fn_handle *h = m->hndl;
  fn_lis *l = fnd_lis(h->tn, h->key_fld, h->key);

  if (!l->rec)
    return model_null_entry(m, l);
  if (l && !h->key[0])
    return model_full_entry(m, l);

  ventry *head = lis_get_val(l, h->key_fld);

  model_read_entry(h->model, l, head);
}

static void generate_lines(Model *m)
{
  /* generate hash set of index,line. */
  ventry *it = m->head;
  for (int i = 0; i < tbl_ent_count(m->head); i++) {
    fn_line ln;
    ln.rec = it->rec;
    utarray_push_back(m->lines, &ln);
    it = it->next;
  }
}

void model_null_entry(Model *m, fn_lis *lis)
{
  log_msg("MODEL", "model_null_entry");
  fn_handle *h = m->hndl;
  m->head = NULL;
  m->cur = NULL;
  lis->index = 0;
  lis->lnum = 0;
  buf_full_invalidate(h->buf, lis->index, lis->lnum);
  m->blocking = false;
}

void model_full_entry(Model *m, fn_lis *lis)
{
  log_msg("MODEL", "model_full_entry");
  fn_rec *it = lis->rec;
  int f = 0; /* make compiler stop complaining */
  while (it) {
    fn_line ln;
    ln.rec = it;
    utarray_insert(m->lines, &ln, f);
    it = tbl_iter(it);
  }
  filter_apply(m->hndl);
  m->lis = lis;
  m->blocking = false;
}

void model_read_entry(Model *m, fn_lis *lis, ventry *head)
{
  log_msg("MODEL", "model_read_entry");
  if (!m->pfval) {
    m->pfval = strdup(lis->fval);
    m->ptop = lis->index;
    m->plnum = lis->lnum;
  }

  m->head = ent_head(head);
  m->cur = head->rec;
  m->lis = lis;
  generate_lines(m);
  filter_apply(m->hndl);
  model_sort(m, (sort_t){-1,0});
  m->blocking = false;
}

char* model_str_line(Model *m, int index)
{
  if (!m->lines || utarray_len(m->lines) < index)
    return NULL;

  fn_line *res = (fn_line*)utarray_eltptr(m->lines, index);
  return res ? rec_fld(res->rec, m->hndl->fname) : NULL;
}

int model_count(Model *m)
{
  return utarray_len(m->lines);
}

void* model_curs_value(Model *m, const char *fld)
{
  return rec_fld(m->cur, fld);
}

void* model_fld_line(Model *m, const char *fld, int index)
{
  fn_line *res = (fn_line*)utarray_eltptr(m->lines, index);
  return rec_fld(res->rec, fld);
}

fn_rec* model_rec_line(Model *m, int index)
{
  fn_line *res = (fn_line*)utarray_eltptr(m->lines, index);
  return res->rec;
}

void model_set_curs(Model *m, int index)
{
  log_msg("MODEL", "model_set_curs");
  if (!m->lines)
    return;

  fn_line *res = (fn_line*)utarray_eltptr(m->lines, index);
  if (res)
    m->cur = res->rec;
}

void model_clear_filter(Model *m)
{
  log_msg("MODEL", "clear filter");
  utarray_clear(m->lines);
  generate_lines(m);
}

void model_filter_line(Model *m, int index)
{
  utarray_erase(m->lines, index, 1);
}

char* model_str_expansion(char *val, char *key)
{
  Buffer *buf = window_get_focus();
  if (!buf || !buf_attached(buf))
    return NULL;

  Model *m = buf->hndl->model;
  return rec_fld(m->cur, val);
}
