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

static void generate_lines(Model *m);

struct fn_line {
  fn_rec *rec;
};
static const UT_icd icd = {sizeof(fn_line),NULL,NULL,NULL};

struct Model {
  fn_handle *hndl;
  fn_lis *lis;
  fn_rec *cur;
  ventry *head;
  bool blocking;
  char *pfval;
  int plnum;
  int ptop;
  int sort_type;
  int sort_rev;
  UT_array *lines;
};

typedef struct {
  int i;
  bool rev;
} sort_t;

static int cmp_str(const void *, const void *, void *);
static int cmp_time(const void *, const void *, void *);
static int cmp_dir(const void *, const void *, void *);
static int cmp_num(const void *, const void *, void *);
//static int cmp_type(const void *, const void *, void *);
typedef struct Sort_T Sort_T;
static struct Sort_T {
  int val;
  int (*cmp)(const void *, const void *, void *);
} sort_tbl[] = {
  {SRT_STR,    cmp_str},
  {SRT_TIME,   cmp_time},
  {SRT_DIR,    cmp_dir},
  {SRT_NUM,    cmp_num},
  //{SRT_TYPE, cmp_type},
};
//TODO: remove field name from cmp functions:
//field name can be passed into each cmp fn within an arg struct.

static int cmp_time(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  time_t t1 = rec_ctime(l1.rec);
  time_t t2 = rec_ctime(l2.rec);
  return difftime(t2, t1);
}

static int cmp_str(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  char *s1 = rec_fld(l1.rec, "name");
  char *s2 = rec_fld(l2.rec, "name");
  return strcmp(s1, s2);
}

static int cmp_dir(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  int b1 = isrecdir(l1.rec);
  int b2 = isrecdir(l2.rec);
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
  return t1 - t2;
}

static int sort_by_type(const void *a, const void *b, void *arg)
{
  sort_t *srt = (sort_t*)arg;
  int ret = sort_tbl[srt->i].cmp(a,b,0);
  if (ret == 0)
    return 0;
  return srt->rev ? -ret : ret;
}

static void do_sort(Model *m)
{
  sort_t srt = {0, m->sort_rev};
  for (int i = 0; i < LENGTH(sort_tbl); i++) {
    int row_type = sort_tbl[i].val;
    if ((m->sort_type & row_type) == row_type) {
      srt.i = i;
      break;
    }
  }
  utarray_sort(m->lines, sort_by_type, &srt);
}

static fn_line* find_by_type(Model *m, fn_line *ln)
{
  sort_t srt = {0, m->sort_rev};

  for (int i = 0; i < LENGTH(sort_tbl); i++) {
    int row_type = sort_tbl[i].val;
    if ((m->sort_type & row_type) == row_type) {
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
  m->sort_rev = 1;
  m->blocking = true;
  utarray_new(m->lines, &icd);
  Buffer *buf = hndl->buf;
  buf->matches = regex_new(hndl);
}

void model_cleanup(fn_handle *hndl)
{
  Model *m = hndl->model;
  regex_destroy(hndl);
  utarray_free(m->lines);
  if (m->pfval)
    free(m->pfval);
  free(m);
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

static void refit(Model *m, fn_lis *lis, Buffer *buf)
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
  fn_line ln = { .rec = it->rec };

  find = find_by_type(m, &ln);

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
  SWAP_ALLOC_PTR(m->pfval, strdup(curval));
  m->ptop = buf_top(buf);
  m->plnum = buf_line(buf);
}

void model_sort(Model *m, int sort_type, int flags)
{
  log_msg("MODEL", "model_sort");
  if (!m->blocking)
    model_set_prev(m);

  if (sort_type != -1) {
    m->sort_type = sort_type;
    m->sort_rev = flags;
  }

  do_sort(m);
  refind_line(m);
  refit(m, m->lis, m->hndl->buf);
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
  if (!m->blocking)
    return;

  fn_handle *h = m->hndl;
  fn_lis *l = fnd_lis(h->tn, h->key_fld, h->key);

  if (!l->rec)
    return model_null_entry(m, l);

  ventry *head = lis_get_val(l, h->key_fld);

  model_read_entry(h->model, l, head);
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

void model_read_entry(Model *m, fn_lis *lis, ventry *head)
{
  log_msg("MODEL", "model_read_entry");
  fn_handle *h = m->hndl;
  if (!m->pfval) {
    m->pfval = strdup(lis->fval);
    m->ptop = lis->index;
    m->plnum = lis->lnum;
  }

  m->head = ent_head(head);
  m->cur = head->rec;
  h->model->lis = lis;
  generate_lines(m);
  model_sort(m, -1, 0);
  m->blocking = false;
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

char* model_str_expansion(char *val, char *key)
{
  Buffer *buf = window_get_focus();
  if (!buf || !buf_attached(buf))
    return NULL;

  Model *m = buf->hndl->model;
  return rec_fld(m->cur, val);
}
