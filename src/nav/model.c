#include "nav/model.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/nav.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/event/fs.h"
#include "nav/option.h"

struct nv_line {
  TblRec *rec;
};
typedef struct {
  int i;
  bool rev;
} sort_ent;

static const UT_icd icd = {sizeof(nv_line),NULL,NULL,NULL};

struct Model {
  Handle *hndl;     //opened handle
  TblLis *lis;      //listener
  TblRec *cur;      //current rec
  Ventry *head;     //head entry of listener
  bool blocking;    //blocking state
  char *pfval;      //prev field value
  int plnum;        //prev lnum
  int ptop;         //prev top
  sort_ent sort;    //current sort type
  int (*sortfn)();
  UT_array *lines;
};
static sort_ent focus = {0,1};

static int cmp_str (TblRec *, TblRec *);
static int cmp_time(TblRec *, TblRec *);
static int cmp_size(TblRec *, TblRec *);
static int cmp_type(TblRec *, TblRec *);
typedef struct Sort_T Sort_T;
static struct Sort_T {
  char *key;
  int (*cmp)();
} sort_tbl[] = {
  {"name",   cmp_str},
  {"ctime",  cmp_time},
  {"size",   cmp_size},
  {"type",   cmp_type},
};

static int cmp_time(TblRec *r1, TblRec *r2)
{
  time_t t1 = rec_ctime(r1);
  time_t t2 = rec_ctime(r2);
  return difftime(t1, t2);
}

static int cmp_str(TblRec *r1, TblRec *r2)
{
  char *s1 = rec_fld(r1, "name");
  char *s2 = rec_fld(r2, "name");
  return strverscmp(s2, s1);
}

static int cmp_size(TblRec *r1, TblRec *r2)
{
  off_t t1 = rec_stsize(r1);
  off_t t2 = rec_stsize(r2);
  if (t1 < t2)
    return -1;
  if (t1 > t2)
    return 1;
  return 0;
}

static int cmp_type(TblRec *r1, TblRec *r2)
{
  char *s1 = rec_fld(r1, "name");
  char *s2 = rec_fld(r2, "name");

  nv_syn *sy1 = get_syn(file_ext(s1));
  nv_syn *sy2 = get_syn(file_ext(s2));
  if (sy1)
    s1 = sy1->group->key;
  if (sy2)
    s2 = sy2->group->key;

  return strverscmp(s2, s1);
}

static int sort_with_stat(const void *a, const void *b, void *arg)
{
  sort_ent *srt = (sort_ent*)arg;
  TblRec *r1 = ((nv_line*)a)->rec;
  TblRec *r2 = ((nv_line*)b)->rec;

  int ret = isrecdir(r1) - isrecdir(r2);

  if (ret == 0)
    ret = sort_tbl[srt->i].cmp(r1, r2, 0);
  if (ret == 0)
    return 0;
  return srt->rev ? ret : -ret;
}

static int sort_basic(const void *a, const void *b, void *arg)
{
  sort_ent *srt = (sort_ent*)arg;
  TblRec *r1 = ((nv_line*)a)->rec;
  TblRec *r2 = ((nv_line*)b)->rec;
  int ret = sort_tbl[srt->i].cmp(r1, r2, 0);
  if (ret == 0)
    return 0;
  return srt->rev ? ret : -ret;
}

static void get_sort_type(Model *m, char *key)
{
  for (int i = 0; i < LENGTH(sort_tbl); i++) {
    if (!strcmp(key, sort_tbl[i].key)) {
      m->sort.i = i;
      break;
    }
  }
}

static nv_line* find_by_type(Model *m, nv_line *ln)
{
  return utarray_find(m->lines, ln, m->sortfn, &m->sort);
}

static nv_line* find_linear(Model *m, const char *val)
{
  for (int i = 0; i < utarray_len(m->lines); i++) {
    nv_line *ln = (nv_line*)utarray_eltptr(m->lines, i);
    if (!strcmp(val, rec_fld(ln->rec, m->hndl->kname)))
      return ln;
  }
  return NULL;
}

void model_init(Handle *hndl)
{
  Model *m = calloc(1, sizeof(Model));
  m->hndl = hndl;
  hndl->model = m;

  m->blocking = true;
  utarray_new(m->lines, &icd);
  Buffer *buf = hndl->buf;
  buf->matches = regex_new(hndl);
  buf->filter = filter_new(hndl);

  //TODO: sort setting
  //if sortinherit
  //  if !sortfield
  //    use default sort num
  //  else
  //    set sort num from field name, incl fail num
  //    set rev
  //else
  //  if !sortfield
  //    use default sort num
  //  else
  //    set sort num from field name, incl fail num
  //    set rev
  char *fld = get_opt_str("sort");
  int rev = get_opt_int("sortreverse");

  if (fld)
    get_sort_type(m, fld);
  focus = m->sort = (sort_ent){m->sort.i, rev};

  if (tbl_types(hndl->tn) & TYP_STAT)
    m->sortfn = sort_with_stat;
  else
    m->sortfn = sort_basic;
}

void model_cleanup(Handle *hndl)
{
  Model *m = hndl->model;
  regex_destroy(hndl);
  filter_destroy(hndl);
  utarray_free(m->lines);
  if (m->pfval)
    free(m->pfval);
  free(m);
}

void model_open(Handle *hndl)
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

void model_close(Handle *hndl)
{
  log_msg("MODEL", "model_close");
  Model *m = hndl->model;
  model_save(m);
  m->blocking = true;
  utarray_clear(m->lines);
}

bool model_blocking(Handle *hndl)
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

static int try_old_pos(Model *m, TblLis *lis, int pos)
{
  nv_line *ln;
  ln = (nv_line*)utarray_eltptr(m->lines, pos);
  if (!ln)
    return 0;
  return (!strcmp(m->pfval, rec_fld(ln->rec, m->hndl->kname)));
}

static void try_old_val(Model *m, TblLis *lis, Ventry *it)
{
  int pos = m->ptop + m->plnum;
  nv_line *find;
  it = ent_rec(it->rec, "dir");
  if (!it)
    return;
  nv_line ln = { .rec = it->rec };

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
  Handle *h = m->hndl;
  TblLis *lis = m->lis;

  Ventry *it = fnd_val(h->tn, m->hndl->kname, m->pfval);
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

void model_ch_focus(Handle *hndl)
{
  if (hndl && hndl->model)
    focus = hndl->model->sort;
}

void model_sort(Model *m, char *key, int flags)
{
  log_msg("MODEL", "model_sort");
  if (!m->blocking)
    model_set_prev(m);

  if (key) {
    get_sort_type(m, key);
    focus = m->sort = (sort_ent){m->sort.i, flags};
  }

  utarray_sort(m->lines, m->sortfn, &m->sort);
  refind_line(m);
  refit(m, m->hndl->buf);
  buf_full_invalidate(m->hndl->buf, m->ptop, m->plnum);
}

void model_flush(Handle *hndl, bool reopen)
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

  Handle *h = m->hndl;
  TblLis *l = fnd_lis(h->tn, h->key_fld, h->key);

  if (!l->rec)
    return model_null_entry(m, l);
  if (l && !h->key[0])
    return model_full_entry(m, l);

  Ventry *head = lis_get_val(l, h->key_fld);

  model_read_entry(h->model, l, head);
}

static void generate_lines(Model *m)
{
  /* generate hash set of index,line. */
  Ventry *it = m->head;
  for (int i = 0; i < tbl_ent_count(m->head); i++) {
    nv_line ln;
    ln.rec = it->rec;
    utarray_push_back(m->lines, &ln);
    it = it->next;
  }
}

void model_null_entry(Model *m, TblLis *lis)
{
  log_msg("MODEL", "model_null_entry");
  Handle *h = m->hndl;
  m->head = NULL;
  m->cur = NULL;
  lis->index = 0;
  lis->lnum = 0;
  buf_full_invalidate(h->buf, lis->index, lis->lnum);
  m->blocking = false;
}

void model_full_entry(Model *m, TblLis *lis)
{
  log_msg("MODEL", "model_full_entry");
  TblRec *it = lis->rec;
  while (it) {
    nv_line ln;
    ln.rec = it;
    int f = 0; /* make compiler stop complaining */
    utarray_insert(m->lines, &ln, f);
    it = tbl_iter(it);
  }
  filter_apply(m->hndl);
  m->lis = lis;
  m->blocking = false;
}

void model_read_entry(Model *m, TblLis *lis, Ventry *head)
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
  model_sort(m, NULL, 0);
  m->blocking = false;
}

char* model_str_line(Model *m, int index)
{
  if (!m->lines || utarray_len(m->lines) < index)
    return NULL;

  nv_line *res = (nv_line*)utarray_eltptr(m->lines, index);
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
  nv_line *res = (nv_line*)utarray_eltptr(m->lines, index);
  return rec_fld(res->rec, fld);
}

TblRec* model_rec_line(Model *m, int index)
{
  nv_line *res = (nv_line*)utarray_eltptr(m->lines, index);
  return res->rec;
}

void model_set_curs(Model *m, int index)
{
  log_msg("MODEL", "model_set_curs");
  if (!m->lines)
    return;

  nv_line *res = (nv_line*)utarray_eltptr(m->lines, index);
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
