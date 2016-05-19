#include "nav/lib/sys_queue.h"
#include "nav/table.h"
#include "nav/log.h"
#include "nav/compl.h"

static ventry* tbl_del_rec(fn_tbl *t, fn_rec *rec, ventry *cur);
static void tbl_insert(fn_tbl *t, trans_rec *trec);

struct fn_val {
  union {
    char *key;
    void *data;
  };
  ventry *rlist;
  fn_fld *fld;
  int count;
  UT_hash_handle hh;
};

struct fn_rec {
  fn_val **vals;
  ventry **vlist;
  int fld_count;
  LIST_ENTRY(fn_rec) ent;
};

struct fn_fld {
  char *key;
  int type;
  fn_val *vals;
  fn_lis *lis;
  UT_hash_handle hh;
};

struct fn_vt_fld {
  char *key;
  int type;
  UT_hash_handle hh;
};

struct fn_tbl {
  char *key;
  fn_fld *fields;
  fn_vt_fld *vtfields;
  int rec_count;
  int vis_fld_count;
  LIST_HEAD(Rec, fn_rec) recs;
  UT_hash_handle hh;
};

static fn_tbl *FN_MASTER;

void tables_init()
{
  log_msg("INIT", "tables_init");
}

void tables_cleanup()
{
  log_msg("CLEANUP", "tables_cleanup");
  fn_tbl *it, *tmp;
  HASH_ITER(hh, FN_MASTER, it, tmp) {
    tbl_del(it->key);
    free(it->key);
    free(it);
  }
}

bool tbl_mk(const char *name)
{
  fn_tbl *t = get_tbl(name);
  if (t)
    return false;

  log_msg("TABLE", "making table {%s} ...", name);
  t = malloc(sizeof(fn_tbl));
  memset(t, 0, sizeof(fn_tbl));
  t->key = strdup(name);
  HASH_ADD_STR(FN_MASTER, key, t);
  return true;
}

void tbl_del(const char *name)
{
  log_msg("CLEANUP", "deleting table {%s} ...", name);
  fn_tbl *t = get_tbl(name);
  if (!t)
    return;

  while (!LIST_EMPTY(&t->recs)) {
    fn_rec *it = LIST_FIRST(&t->recs);
    for (int itf = 0; itf < it->fld_count; itf++) {
      fn_fld *fld = it->vals[itf]->fld;
      fn_val *val = it->vals[itf];

      val->count--;
      free(it->vlist[itf]);
      if (val->count < 1) {
        if (fld->type == TYP_VOID)
          free(val->data);
        else {
          HASH_DEL(fld->vals, val);
          free(val->key);
        }
        free(val);
      }
    }
    LIST_REMOVE(it, ent);
    free(it->vlist);
    free(it->vals);
    free(it);
  }

  fn_fld *f, *ftmp;
  HASH_ITER(hh, t->fields, f, ftmp) {
    HASH_DEL(t->fields, f);
    log_msg("CLEANUP", "deleting field {%s} ...", f->key);
    tbl_del_fld_lis(f);
    free(f->key);
    free(f);
  }
  fn_vt_fld *vf, *vftmp;
  HASH_ITER(hh, t->vtfields, vf, vftmp) {
    HASH_DEL(t->vtfields, vf);
    log_msg("CLEANUP", "deleting field {%s} ...", vf->key);
    free(vf->key);
    free(vf);
  }
  HASH_DEL(FN_MASTER, t);
}

fn_tbl* get_tbl(const char *tn)
{
  fn_tbl *ret;
  HASH_FIND_STR(FN_MASTER, tn, ret);
  if (!ret)
    log_msg("ERROR", "::get_tbl %s does not exist", tn);
  return ret;
}

int fld_sort(fn_fld *a, fn_fld *b)
{
  return strcmp(a->key, b->key);
}

void tbl_mk_fld(const char *tn, const char *name, int type)
{
  log_msg("TABLE", "making {%s} field {%s} ...", tn, name);
  fn_tbl *t = get_tbl(tn);
  fn_fld *fld = malloc(sizeof(fn_fld));
  memset(fld, 0, sizeof(fn_fld));
  fld->key = strdup(name);
  fld->type = type;
  if (type != TYP_VOID)
    t->vis_fld_count++;
  HASH_ADD_STR(t->fields, key, fld);
  HASH_SORT(t->fields, fld_sort);
  log_msg("TABLE", "made %s", fld->key);
}

void tbl_mk_vt_fld(const char *tn, const char *name, int type)
{
  log_msg("TABLE", "making {%s} vt_field {%s} ...", tn, name);
  fn_tbl *t = get_tbl(tn);
  fn_vt_fld *fld = malloc(sizeof(fn_vt_fld));
  memset(fld, 0, sizeof(fn_vt_fld));
  fld->key = strdup(name);
  fld->type = type;
  if (type != TYP_VOID)
    t->vis_fld_count++;
  HASH_ADD_STR(t->vtfields, key, fld);
  log_msg("TABLE", "made %s", fld->key);
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals  = malloc(HASH_COUNT(t->fields) * sizeof(fn_val*));
  rec->vlist = malloc(HASH_COUNT(t->fields) * sizeof(fn_val*));
  rec->fld_count = HASH_COUNT(t->fields);
  return rec;
}

ventry* fnd_val(const char *tn, const char *fld, const char *val)
{
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return NULL;

  fn_val *v;
  HASH_FIND_STR(f->vals, val, v);
  if (v)
    return v->rlist;

  return NULL;
}

fn_lis* fnd_lis(const char *tn, const char *fld, const char *key)
{
  log_msg("TABLE", "fnd_lis %s %s", fld, key);
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return NULL;

  fn_lis *l;
  HASH_FIND_STR(f->lis, key, l);
  if (!l)
    return f->lis;

  return l;
}

ventry* lis_get_val(fn_lis *lis, const char *fld)
{
  log_msg("TABLE", "lis_get_val");
  for(int i = 0; i < lis->rec->fld_count; i++) {
    ventry *it = lis->rec->vlist[i];
    if (!it)
      continue;

    if (strcmp(it->val->fld->key, fld) == 0)
      return it;
  }
  return NULL;
}

void lis_save(fn_lis *lis, int index, int lnum, const char *fval)
{
  lis->index = index;
  lis->lnum = lnum;
  SWAP_ALLOC_PTR(lis->fval, strdup(fval));
}

void* rec_fld(fn_rec *rec, const char *fld)
{
  if (!rec)
    return NULL;

  for(int i = 0; i < rec->fld_count; i++) {
    fn_val *val = rec->vals[i];
    if (strcmp(val->fld->key, fld) != 0)
      continue;

    if (val->fld->type == TYP_STR)
      return val->key;
    else
      return val->data;
  }
  return NULL;
}

ventry* ent_rec(fn_rec *rec, const char *fld)
{
  for(int i = 0; i < rec->fld_count; i++) {
    fn_val *val = rec->vals[i];
    if (!val)
      continue;
    if (strcmp(val->fld->key, fld) != 0)
      continue;

    return rec->vlist[i];
  }
  return NULL;
}

int fld_type(const char *tn, const char *fld)
{
  fn_tbl *t = get_tbl(tn);
  if (!tn)
    return -1;

  fn_fld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (f)
    return f->type;

  fn_vt_fld *vf;
  HASH_FIND_STR(t->vtfields, fld, vf);
  if (vf)
    return vf->type;

  return -1;
}

int tbl_fld_count(const char *tn)
{
  return HASH_COUNT(get_tbl(tn)->fields);
}

int tbl_ent_count(ventry *e)
{
  return e->val->count;
}

char* ent_str(ventry *ent)
{
  return ent->val->key;
}

ventry* ent_head(ventry *ent)
{
  return ent->val->rlist;
}

void tbl_del_val(const char *tn, const char *fld, const char *val)
{
  log_msg("TABLE", "delete_val() %s %s,%s",tn ,fld, val);
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return;

  fn_val *v;
  HASH_FIND_STR(f->vals, val, v);
  if (!v)
    return;

  fn_lis *l;
  HASH_FIND_STR(f->lis, val, l);
  if (l)
    l->rec = NULL;

  /* iterate entries of val. */
  ventry *it = v->rlist;
  int count = v->count;
  for (int i = 0; i < count; i++)
    it = tbl_del_rec(t, it->rec, it);
}

void tbl_add_lis(const char *tn, const char *fld, const char *key)
{
  log_msg("TABLE", "SET LIS %s|%s", fld, key);

  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return;

  /* don't create listener is already exists */
  fn_lis *l;
  HASH_FIND_STR(f->lis, key, l);
  if (l)
    return;

  /* create new listener */
  log_msg("TABLE", "new lis");
  l = calloc(1, sizeof(fn_lis));
  l->key = strdup(key);
  l->key_fld = f;
  l->fval = strdup("");
  HASH_ADD_STR(f->lis, key, l);

  /* check if value exists and attach */
  fn_val *val;
  HASH_FIND_STR(f->vals, key, val);
  if (val)
    l->rec = val->rlist->rec;
}

void tbl_del_fld_lis(fn_fld *f)
{
  fn_lis *it, *tmp;
  HASH_ITER(hh, f->lis, it, tmp) {
    HASH_DEL(f->lis, it);
    free(it->key);
    free(it->fval);
    free(it);
  }
}

void commit(void **data)
{
  log_msg("TABLE", "commit");
  fn_tbl *t = get_tbl(data[0]);
  tbl_insert(t, data[1]);
  clear_trans(data[1], 0);
}

static fn_val* new_entry(fn_rec *rec, fn_fld *fld, void *data, int typ, int indx)
{
  fn_val *val = malloc(sizeof(fn_val));
  val->fld = fld;
  if (typ) {
    val->count = 1;
    val->data = data;
    rec->vals[indx] = val;
  }
  else {
    ventry *ent = malloc(sizeof(ventry));
    val->count = 1;
    ent->next = ent;
    ent->prev = ent;

    val->rlist = ent;
    val->key = strdup(data);

    ent->rec = rec;
    ent->val = val;
    rec->vlist[indx] = ent;
    rec->vals[indx] = val;
    HASH_ADD_STR(fld->vals, key, val);
  }
  return val;
}

static void add_entry(fn_rec *rec, fn_fld *fld, fn_val *v, int typ, int indx)
{
  /* attach record to an entry. */
  ventry *ent = malloc(sizeof(ventry));
  v->count++;
  ent->rec = rec;
  ent->val = v;
  ent->prev = v->rlist->prev;
  ent->next = v->rlist;
  v->rlist->prev->next = ent;
  v->rlist->prev = ent;
  rec->vlist[indx] = ent;
  rec->vals[indx] = v;
}

static void check_set_lis(fn_fld *f, const char *key, fn_rec *rec)
{
  /* check if field has lis. */
  if (HASH_COUNT(f->lis) < 1)
    return;

  fn_lis *ll;
  HASH_FIND_STR(f->lis, key, ll);
  if (!ll || ll->rec)
    return;

  /* if lis hasn't obtained a rec, set it now. */
  log_msg("TABLE", "SET LIS");
  ll->rec = rec;
}

static void tbl_insert(fn_tbl *t, trans_rec *trec)
{
  t->rec_count++;
  fn_rec *rec = mk_rec(t);

  for(int i = 0; i < rec->fld_count; i++) {
    void *data = trec->data[i];

    fn_fld *f;
    HASH_FIND_STR(t->fields, trec->flds[i], f);

    if (f->type == TYP_VOID) {
      rec->vals[i] = new_entry(rec, f, data, 1, i);
      rec->vlist[i] = NULL;
      continue;
    }

    fn_val *v;
    HASH_FIND_STR(f->vals, data, v);

    /* create header entry. */
    if (!v)
      new_entry(rec, f, data, 0, i);
    else
      add_entry(rec, f, v, 0, i);

    check_set_lis(f, rec->vals[i]->key, rec);
  }
  LIST_INSERT_HEAD(&t->recs, rec, ent);
}

static void del_fldval(fn_fld *fld, fn_val *val)
{
  fn_val *fnd;
  HASH_FIND_STR(fld->vals, val->key, fnd);
  if (fnd)
    HASH_DEL(fld->vals, val);

  fn_lis *ll;
  HASH_FIND_STR(fld->lis, val->key, ll);
  if (ll)
    ll->rec = NULL;

  if (fld->type == TYP_VOID)
    free(val->data);
  free(val->key);
  val->rlist = NULL;
}

static void pop_ventry(ventry *it, fn_val *val, ventry **cur)
{
  log_msg("TABLE", "popped");
  if (it == val->rlist)
    val->rlist = it->next;
  if (*cur == it)
    *cur = val->rlist;

  it->next->prev = it->prev;
  it->prev->next = it->next;

  free(it);
  it = NULL;
}

static ventry* tbl_del_rec(fn_tbl *t, fn_rec *rec, ventry *cur)
{
  log_msg("TABLE", "delete_rec()");
  for(int i = 0; i < rec->fld_count; i++) {
    ventry *it = rec->vlist[i];
    if (!it) {
      if (rec->vals[i]->fld->type == TYP_VOID)
        free(rec->vals[i]->data);
      free(rec->vals[i]);
    }
    else {
      fn_val *val = rec->vals[i];
      it->val->count--;

      if (val->count < 1) {
        fn_fld *fld = val->fld;

        del_fldval(fld, val);
        free(rec->vals[i]);
        rec->vals[i] = NULL;

        if (cur == it)
          cur = NULL;

        free(it);
      }
      else
        pop_ventry(it, val, &cur);
    }
  }
  LIST_REMOVE(rec, ent);
  free(rec->vals);
  free(rec->vlist);
  free(rec);
  return cur;
}

trans_rec* mk_trans_rec(int fld_count)
{
  trans_rec *r = malloc(sizeof(trans_rec));
  r->max = fld_count;
  r->flds = malloc(r->max * sizeof(char*));
  r->data = malloc(r->max * sizeof(void*));
  r->type = malloc(r->max * sizeof(int));
  r->count = 0;
  return r;
}

void edit_trans(trans_rec *r, char *fld, char *val, void *data)
{
  r->flds[r->count] = fld;
  if (val) {
    r->data[r->count] = strdup(val);
    r->type[r->count] = 1;
  }
  else {
    r->data[r->count] = data;
    r->type[r->count] = 0;
  }
  r->count++;
}

void clear_trans(trans_rec *r, int flush)
{
  for (int i = 0; i < r->count; i++) {
    if (r->type[i] == 1 || flush)
      free(r->data[i]);
  }
  free(r->type);
  free(r->flds);
  free(r->data);
  free(r);
}

void field_list(List *args)
{
  if (HASH_COUNT(FN_MASTER) < 1)
    return;

  fn_tbl *t = get_tbl("fm_files");

  fn_fld *it;
  int i = 0;
  for (it = t->fields; it != NULL; it = it->hh.next) {
    if (it->type == TYP_VOID)
      continue;
    compl_list_add("%s", it->key);
    i++;
  }
  fn_vt_fld *vit;
  for (vit = t->vtfields; vit != NULL; vit = vit->hh.next) {
    if (vit->type == TYP_VOID)
      continue;
    compl_list_add("%s", vit->key);
    i++;
  }
}
