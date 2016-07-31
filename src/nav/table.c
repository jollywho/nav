#include "nav/lib/sys_queue.h"
#include "nav/table.h"
#include "nav/log.h"
#include "nav/compl.h"

static Ventry* tbl_del_rec(Table *t, TblRec *rec, Ventry *cur);
static void tbl_insert(Table *t, trans_rec *trec);

struct TblVal {
  union {
    char *key;
    void *data;
  };
  Ventry *rlist;
  TblFld *fld;
  int count;
  UT_hash_handle hh;
};

struct TblRec {
  TblVal **vals;
  Ventry **vlist;
  int fld_count;
  LIST_ENTRY(TblRec) ent;
};

struct TblFld {
  char *key;
  int type;
  TblVal *vals;
  TblLis *lis;
  UT_hash_handle hh;
};

struct Table {
  char *key;
  TblFld *fields;
  int srt_types;
  int rec_count;
  LIST_HEAD(Rec, TblRec) recs;
  UT_hash_handle hh;
};

static Table *NV_MASTER;

void tables_init()
{
  log_msg("INIT", "tables_init");
}

void tables_cleanup()
{
  log_msg("CLEANUP", "tables_cleanup");
  Table *it, *tmp;
  HASH_ITER(hh, NV_MASTER, it, tmp) {
    tbl_del(it->key);
  }
}

bool tbl_mk(const char *name)
{
  Table *t = get_tbl(name);
  if (t)
    return false;

  log_msg("TABLE", "making table {%s} ...", name);
  t = malloc(sizeof(Table));
  memset(t, 0, sizeof(Table));
  t->key = strdup(name);
  HASH_ADD_STR(NV_MASTER, key, t);
  return true;
}

void tbl_del(const char *name)
{
  log_msg("CLEANUP", "deleting table {%s} ...", name);
  Table *t = get_tbl(name);
  if (!t)
    return;

  while (!LIST_EMPTY(&t->recs)) {
    TblRec *it = LIST_FIRST(&t->recs);
    for (int itf = 0; itf < it->fld_count; itf++) {
      TblFld *fld = it->vals[itf]->fld;
      TblVal *val = it->vals[itf];

      val->count--;
      free(it->vlist[itf]);
      if (val->count < 1) {
        if (BITMASK_CHECK(fld->type, TYP_STAT))
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

  TblFld *f, *ftmp;
  HASH_ITER(hh, t->fields, f, ftmp) {
    HASH_DEL(t->fields, f);
    log_msg("CLEANUP", "deleting field {%s} ...", f->key);
    tbl_del_fld_lis(f);
    free(f->key);
    free(f);
  }
  HASH_DEL(NV_MASTER, t);
  free(t->key);
  free(t);
}

Table* get_tbl(const char *tn)
{
  Table *ret;
  HASH_FIND_STR(NV_MASTER, tn, ret);
  if (!ret)
    log_msg("ERROR", "::table %s does not exist", tn);
  return ret;
}

void tbl_mk_fld(const char *tn, const char *name, int type)
{
  log_msg("TABLE", "making {%s} field {%s} ...", tn, name);
  Table *t = get_tbl(tn);
  TblFld *fld = calloc(1, sizeof(TblFld));
  fld->key = strdup(name);
  fld->type = type;
  t->srt_types |= type;
  HASH_ADD_STR(t->fields, key, fld);
  log_msg("TABLE", "made %s", fld->key);
}

TblRec* mk_rec(Table *t)
{
  TblRec *rec = malloc(sizeof(TblRec));
  rec->vals  = malloc(HASH_COUNT(t->fields) * sizeof(TblVal*));
  rec->vlist = malloc(HASH_COUNT(t->fields) * sizeof(TblVal*));
  rec->fld_count = HASH_COUNT(t->fields);
  return rec;
}

Ventry* fnd_val(const char *tn, const char *fld, const char *val)
{
  Table *t = get_tbl(tn);
  TblFld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return NULL;

  TblVal *v;
  HASH_FIND_STR(f->vals, val, v);
  if (v)
    return v->rlist;

  return NULL;
}

TblLis* fnd_lis(const char *tn, const char *fld, const char *key)
{
  log_msg("TABLE", "fnd_lis %s %s", fld, key);
  Table *t = get_tbl(tn);
  TblFld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return NULL;

  TblLis *l;
  HASH_FIND_STR(f->lis, key, l);
  if (!l)
    return f->lis;

  if (!key[0])
    l->rec = LIST_FIRST(&t->recs);

  return l;
}

Ventry* lis_get_val(TblLis *lis, const char *fld)
{
  log_msg("TABLE", "lis_get_val");
  TblRec *rec = lis->rec;
  for(int i = 0; i < rec->fld_count; i++) {
    Ventry *it = rec->vlist[i];
    if (!it)
      continue;

    if (strcmp(it->val->fld->key, fld) == 0)
      return it;
  }
  return NULL;
}

void lis_save(TblLis *lis, int index, int lnum, const char *fval)
{
  lis->index = index;
  lis->lnum = lnum;
  SWAP_ALLOC_PTR(lis->fval, strdup(fval));
}

void* rec_fld(TblRec *rec, const char *fld)
{
  if (!rec)
    return NULL;

  for(int i = 0; i < rec->fld_count; i++) {
    TblVal *val = rec->vals[i];
    if (strcmp(val->fld->key, fld) != 0)
      continue;

    if (BITMASK_CHECK(val->fld->type, TYP_STR))
      return val->key;
    else
      return val->data;
  }
  return NULL;
}

Ventry* ent_rec(TblRec *rec, const char *fld)
{
  for(int i = 0; i < rec->fld_count; i++) {
    TblVal *val = rec->vals[i];
    if (!val)
      continue;
    if (strcmp(val->fld->key, fld) != 0)
      continue;

    return rec->vlist[i];
  }
  return NULL;
}

TblRec* tbl_iter(TblRec *next)
{
  return LIST_NEXT(next, ent);
}

int fld_type(const char *tn, const char *fld)
{
  Table *t = get_tbl(tn);
  if (!tn)
    return -1;

  TblFld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (f)
    return f->type;

  return -1;
}

int tbl_types(const char *tn)
{
  return (get_tbl(tn)->srt_types);
}

int tbl_fld_count(const char *tn)
{
  return HASH_COUNT(get_tbl(tn)->fields);
}

int tbl_ent_count(Ventry *e)
{
  return e->val->count;
}

char* tbl_fld(Table *t, int idx)
{
  TblFld *it = t->fields;
  for (int i = 0; i < idx; i++) {
    if (it->hh.next)
      it = it->hh.next;
  }
  return it->key;
}

char* ent_str(Ventry *ent)
{
  return ent->val->key;
}

Ventry* ent_head(Ventry *ent)
{
  return ent->val->rlist;
}

void tbl_del_val(const char *tn, const char *fld, const char *val)
{
  log_msg("TABLE", "delete_val() %s %s,%s",tn ,fld, val);
  Table *t = get_tbl(tn);
  TblFld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return;

  TblVal *v;
  HASH_FIND_STR(f->vals, val, v);
  if (!v)
    return;

  TblLis *l;
  HASH_FIND_STR(f->lis, val, l);
  if (l)
    l->rec = NULL;

  /* iterate entries of val. */
  Ventry *it = v->rlist;
  int count = v->count;
  for (int i = 0; i < count; i++)
    it = tbl_del_rec(t, it->rec, it);
}

void tbl_add_lis(const char *tn, const char *fld, const char *key)
{
  log_msg("TABLE", "SET LIS %s|%s", fld, key);

  Table *t = get_tbl(tn);
  TblFld *f;
  HASH_FIND_STR(t->fields, fld, f);
  if (!f)
    return;

  /* don't create listener is already exists */
  TblLis *l;
  HASH_FIND_STR(f->lis, key, l);
  if (l)
    return;

  /* create new listener */
  log_msg("TABLE", "new lis");
  l = calloc(1, sizeof(TblLis));
  l->key = strdup(key);
  l->key_fld = f;
  l->fval = strdup("");
  HASH_ADD_STR(f->lis, key, l);

  /* check if value exists and attach */
  TblVal *val;
  HASH_FIND_STR(f->vals, key, val);
  if (val)
    l->rec = val->rlist->rec;
}

void tbl_del_fld_lis(TblFld *f)
{
  TblLis *it, *tmp;
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
  Table *t = get_tbl(data[0]);
  tbl_insert(t, data[1]);
  clear_trans(data[1], 0);
}

static TblVal* new_entry(TblRec *rec, TblFld *fld, void *data, int typ, int indx)
{
  TblVal *val = malloc(sizeof(TblVal));
  val->fld = fld;
  if (typ) {
    val->count = 1;
    val->data = data;
    rec->vals[indx] = val;
  }
  else {
    Ventry *ent = malloc(sizeof(Ventry));
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

static void add_entry(TblRec *rec, TblFld *fld, TblVal *v, int typ, int indx)
{
  /* attach record to an entry. */
  Ventry *ent = malloc(sizeof(Ventry));
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

static void check_set_lis(TblFld *f, const char *key, TblRec *rec)
{
  /* check if field has lis. */
  if (HASH_COUNT(f->lis) < 1)
    return;

  TblLis *ll;
  HASH_FIND_STR(f->lis, key, ll);
  if (!ll || ll->rec)
    return;

  /* if lis hasn't obtained a rec, set it now. */
  log_msg("TABLE", "SET LIS");
  ll->rec = rec;
}

static void tbl_insert(Table *t, trans_rec *trec)
{
  t->rec_count++;
  TblRec *rec = mk_rec(t);

  for(int i = 0; i < rec->fld_count; i++) {
    void *data = trec->data[i];

    TblFld *f;
    HASH_FIND_STR(t->fields, trec->flds[i], f);

    if (BITMASK_CHECK(f->type, TYP_STAT)) {
      rec->vals[i] = new_entry(rec, f, data, 1, i);
      rec->vlist[i] = NULL;
      continue;
    }

    TblVal *v;
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

static void del_fldval(TblFld *fld, TblVal *val)
{
  TblVal *fnd;
  HASH_FIND_STR(fld->vals, val->key, fnd);
  if (fnd)
    HASH_DEL(fld->vals, val);

  TblLis *ll;
  HASH_FIND_STR(fld->lis, val->key, ll);
  if (ll)
    ll->rec = NULL;

  if (BITMASK_CHECK(fld->type, TYP_STAT))
    free(val->data);
  free(val->key);
  val->rlist = NULL;
}

static void pop_ventry(Ventry *it, TblVal *val, Ventry **cur)
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

static Ventry* tbl_del_rec(Table *t, TblRec *rec, Ventry *cur)
{
  log_msg("TABLE", "delete_rec()");
  for(int i = 0; i < rec->fld_count; i++) {
    Ventry *it = rec->vlist[i];
    if (!it) {
      if (BITMASK_CHECK(rec->vals[i]->fld->type, TYP_STAT))
        free(rec->vals[i]->data);
      free(rec->vals[i]);
    }
    else {
      TblVal *val = rec->vals[i];
      it->val->count--;

      if (val->count < 1) {
        TblFld *fld = val->fld;

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

void record_list(const char *tn, char *f1, char *f2)
{
  Table *t = get_tbl(tn);
  if (!t)
    return;

  int i = 0;
  TblRec *it;
  LIST_FOREACH(it, &t->recs, ent) {
    compl_list_add("%s", rec_fld(it, f1));
    if (f2)
      compl_set_col(i, "%s", rec_fld(it, f2));
    i++;
  }
}
