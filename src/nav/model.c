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

struct Model {
  fn_handle *hndl;
  fn_lis *lis;
  fn_rec *cur;
  ventry *head;
  bool blocking;
  bool opened;
  char *sort_type;
  int sort_rev;
  UT_array *lines;
};

#define REV_FN(cond,fn,a,b) ((cond) ? (fn((b),(a))) : (fn((a),(b))))

static const UT_icd icd = {sizeof(fn_line),NULL,NULL,NULL};
#define intcmp(a,b) ((a) - (b))

static int date_cmp(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;

  time_t *t1 = fs_vt_stat_resolv(l1.rec, "mtime");
  time_t *t2 = fs_vt_stat_resolv(l2.rec, "mtime");
  return REV_FN(*(int*)arg, difftime, *t2, *t1);
}

static int str_cmp(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  char *s1 = rec_fld(l1.rec, "name");
  char *s2 = rec_fld(l2.rec, "name");
  return REV_FN(*(int*)arg, strcmp, s1, s2);
}

static int dir_cmp(const void *a, const void *b, void *arg)
{
  fn_line l1 = *(fn_line*)a;
  fn_line l2 = *(fn_line*)b;
  char *s1 = rec_fld(l1.rec, "fullpath");
  char *s2 = rec_fld(l2.rec, "fullpath");
  int b1 = fs_vt_isdir_resolv(s1);
  int b2 = fs_vt_isdir_resolv(s2);
  return REV_FN(*(int*)arg, intcmp, b2, b1);
}

void model_init(fn_handle *hndl)
{
  Model *model = malloc(sizeof(Model));
  memset(model, 0, sizeof(Model));
  model->hndl = hndl;
  hndl->model = model;
  model->sort_type = strdup("");
  model->sort_rev = 1;
  model->blocking = true;
  utarray_new(model->lines, &icd);
  Buffer *buf = hndl->buf;
  buf->matches = regex_new(hndl);
}

void model_cleanup(fn_handle *hndl)
{
  Model *m = hndl->model;
  regex_destroy(hndl);
  utarray_free(m->lines);
  free(m->sort_type);
  free(m);
}

void model_open(fn_handle *hndl)
{
  tbl_add_lis(hndl->tn, hndl->key_fld, hndl->key);
}

void model_close(fn_handle *hndl)
{
  log_msg("MODEL", "model_close");
  Model *m = hndl->model;
  Buffer *b = hndl->buf;
  m->blocking = true;
  m->opened = true;
  lis_save(m->lis, buf_top(b), buf_line(b));
  utarray_clear(m->lines);
}

bool model_blocking(fn_handle *hndl)
{
  return hndl->model->blocking;
}

static void refit(Model *m, fn_lis *lis, Buffer *buf)
{
  int pos = lis->index + lis->lnum;
  if (buf_size(m->hndl->buf).lnum > model_count(m)) {
    int dif = lis->index;
    lis->index = 0;
    lis->lnum += dif;
  }
  if (pos >= model_count(m))
    lis->lnum = model_count(m) - lis->index - 1;
}

static int try_old_pos(Model *m, fn_lis *lis, int pos)
{
  fn_line *ln;
  ln = (fn_line*)utarray_eltptr(m->lines, pos);
  if (!ln)
    return 0;
  return (strcmp(lis->fval, rec_fld(ln->rec, m->hndl->kname)) == 0);
}

static void try_old_val(Model *m, fn_lis *lis, ventry *it)
{
  int pos = lis->index + lis->lnum;
  fn_line *find;
  it = ent_rec(it->rec, "dir");
  fn_line ln = { .rec = it->rec };

  if (strcmp(m->sort_type, "mtime") == 0)
    find = (fn_line*)utarray_find(m->lines, &ln, date_cmp, &m->sort_rev);
  else if (strcmp(m->sort_type, "dir") == 0)
    find = (fn_line*)utarray_find(m->lines, &ln, dir_cmp, &m->sort_rev);
  else
    find = (fn_line*)utarray_find(m->lines, &ln, str_cmp, &m->sort_rev);

  if (!find)
    return;

  int foundpos = utarray_eltidx(m->lines, find);
  lis->index = MAX(0, lis->index + (foundpos - pos));
  lis->lnum = MAX(0, foundpos - lis->index);
}

void refind_line(Model *m)
{
  log_msg("MODEL", "refind_line");
  fn_handle *h = m->hndl;
  fn_lis *lis = m->lis;

  ventry *it = fnd_val(h->tn, m->hndl->kname, lis->fval);
  if (!it)
    return;

  if (try_old_pos(m, lis, lis->index + lis->lnum))
    return;

  //FIXME: use one struct and one comparison fn
  //atm, similar timestamps produce incorrect results
  try_old_val(m, lis, it);
}

void model_sort(Model *m, const char *fld, int flags)
{
  log_msg("MODEL", "model_sort");
  if (fld) {
    free(m->sort_type);
    m->sort_type = strdup(fld);
  }
  m->sort_rev = flags;

  if (strcmp(m->sort_type, "mtime") == 0)
    utarray_sort(m->lines, date_cmp, &m->sort_rev);
  else if (strcmp(m->sort_type, "dir") == 0)
    utarray_sort(m->lines, dir_cmp, &m->sort_rev);
  else if (strcmp(m->sort_type, "name") == 0)
    utarray_sort(m->lines, str_cmp, &m->sort_rev);

  refind_line(m);
  refit(m, m->lis, m->hndl->buf);
  buf_full_invalidate(m->hndl->buf, m->lis->index, m->lis->lnum);
}

void model_recv(Model *m)
{
  if (m->opened)
    model_close(m->hndl);

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
  buf_full_invalidate(h->buf, lis->index, lis->lnum);
  m->blocking = false;
  m->opened = true;
}

void model_read_entry(Model *m, fn_lis *lis, ventry *head)
{
  log_msg("MODEL", "model_read_entry");
  fn_handle *h = m->hndl;

  m->head = ent_head(head);
  m->cur = head->rec;
  h->model->lis = lis;
  generate_lines(m);
  model_sort(m, NULL, 0);
  m->blocking = false;
  m->opened = true;
}

static void generate_lines(Model *m)
{
  /* generate hash set of index,line. */
  ventry *it = m->head->next;
  for (int i = 0; i < tbl_ent_count(m->head); ++i) {
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

void model_set_curs(Model *m, int index)
{
  log_msg("MODEL", "model_set_curs");
  if (!m->lines)
    return;

  fn_line *res = (fn_line*)utarray_eltptr(m->lines, index);
  if (res) {
    m->cur = res->rec;
    char *curval = model_curs_value(m, m->hndl->kname);
    SWAP_ALLOC_PTR(m->lis->fval, strdup(curval));
  }
}

char* model_str_expansion(char *val)
{
  Buffer *buf = window_get_focus();
  if (!buf || !buf_attached(buf))
    return NULL;

  Model *m = buf->hndl->model;
  return rec_fld(m->cur, val);
}
