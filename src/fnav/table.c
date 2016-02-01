#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "fnav/table.h"
#include "fnav/log.h"
#include "fnav/compl.h"

fn_tbl* get_tbl(String t);
static ventry* tbl_del_rec(fn_rec *rec, ventry *cur);
static void tbl_insert(fn_tbl *t, trans_rec *trec);

struct fn_val {
  union {
    String key;
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
};

struct fn_fld {
  String key;
  tFldType type;
  fn_val *vals;
  fn_lis *lis;
  UT_hash_handle hh;
};

struct fn_vt_fld {
  String key;
  tbl_vt_cb cb;
  UT_hash_handle hh;
};

struct fn_tbl {
  String key;
  fn_fld *fields;
  fn_vt_fld *vtfields;
  int count;
  int rec_count;
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

bool tbl_mk(String name)
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

void tbl_del(String name)
{
  log_msg("CLEANUP", "deleting table {%s} ...", name);
  fn_tbl *t = get_tbl(name);
  if (!t)
    return;

  fn_fld *f, *ftmp;
  HASH_ITER(hh, t->fields, f, ftmp) {
    HASH_DEL(t->fields, f);
    log_msg("CLEANUP", "deleting field {%s} ...", f->key);
    //TODO: del val
    //TODO: del lis
    //TODO: del ent
    //TODO: del rec
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

fn_tbl* get_tbl(String t)
{
  fn_tbl *ret;
  HASH_FIND_STR(FN_MASTER, t, ret);
  if (!ret)
    log_msg("ERROR", "::get_tbl %s does not exist", t);
  return ret;
}

void tbl_mk_fld(String tn, String name, tFldType typ)
{
  log_msg("TABLE", "making {%s} field {%s} ...", tn, name);
  fn_tbl *t = get_tbl(tn);
  fn_fld *fld = malloc(sizeof(fn_fld));
  memset(fld, 0, sizeof(fn_fld));
  fld->key = strdup(name);
  fld->type = typ;
  t->count++;
  HASH_ADD_STR(t->fields, key, fld);
  log_msg("TABLE", "made %s", fld->key);
}

void tbl_mk_vt_fld(String tn, String name, tbl_vt_cb cb)
{
  log_msg("TABLE", "making {%s} vt_field {%s} ...", tn, name);
  fn_tbl *t = get_tbl(tn);
  fn_vt_fld *fld = malloc(sizeof(fn_vt_fld));
  memset(fld, 0, sizeof(fn_vt_fld));
  fld->key = strdup(name);
  fld->cb = cb;
  HASH_ADD_STR(t->vtfields, key, fld);
  log_msg("TABLE", "made %s", fld->key);
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals  = malloc(t->count * sizeof(fn_val*));
  rec->vlist = malloc(t->count * sizeof(fn_val*));
  rec->fld_count = t->count;
  return rec;
}

ventry* fnd_val(String tn, String fname, String val)
{
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fname, f);
  if (!f)
    return NULL;

  fn_val *v;
  HASH_FIND_STR(f->vals, val, v);
  if (v)
    return v->rlist;

  return NULL;
}

fn_lis* fnd_lis(String tn, String key_fld, String key)
{
  log_msg("TABLE", "fnd_lis %s %s", key_fld, key);
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, key_fld, f);
  if (!f)
    return NULL;

  fn_lis *l;
  HASH_FIND_STR(f->lis, key, l);
  return l;
}

ventry* lis_set_val(fn_lis *lis, String fname)
{
  log_msg("TABLE", "lis_set_val");
  for(int i = 0; i < lis->rec->fld_count; i++) {
    ventry *it = lis->rec->vlist[i];
    if (!it)
      continue;

    fn_val *val = it->val;
    if (strcmp(val->fld->key, fname) == 0) {
      lis->ent = it;
      lis->fname = val->fld;
      lis->fval = val->key;
      return it;
    }
  }
  return NULL;
}

ventry* lis_get_val(fn_lis *lis, String fname)
{
  log_msg("TABLE", "lis_get_val");
  for(int i = 0; i < lis->rec->fld_count; i++) {
    ventry *it = lis->rec->vlist[i];
    if (!it)
      continue;

    if (strcmp(it->val->fld->key, fname) == 0)
      return it;
  }
  return NULL;
}

void lis_save(fn_lis *lis, int index, int lnum)
{
  lis->index = index;
  lis->lnum = lnum;
}

void* rec_fld(fn_rec *rec, String fname)
{
  if (!rec)
    return NULL;

  for(int i = 0; i < rec->fld_count; i++) {
    fn_val *val = rec->vals[i];
    if (strcmp(val->fld->key, fname) != 0)
      continue;

    if (val->fld->type == typSTRING)
      return val->key;
    else
      return val->data;
  }
  return NULL;
}

void* rec_vt_fld(String tn, fn_rec *rec, String fname)
{
  fn_tbl *t = get_tbl(tn);
  fn_vt_fld *f;
  HASH_FIND_STR(t->vtfields, fname, f);
  return f->cb(rec, fname);
}

int tbl_fld_count(String tn)
{
  return get_tbl(tn)->count;
}

int tbl_ent_count(ventry *e)
{
  return e->val->count;
}

String ent_str(ventry *ent)
{
  return ent->val->key;
}

ventry* ent_head(ventry *ent)
{
  return ent->val->rlist;
}

void tbl_del_val(String tn, String fname, String val)
{
  log_msg("TABLE", "delete_val() %s %s,%s",tn ,fname, val);
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fname, f);
  if (!f)
    return;

  fn_val *v;
  HASH_FIND_STR(f->vals, val, v);
  if (!v)
    return;

  /* iterate entries of val. */
  ventry *it = v->rlist;
  while (it)
    it = tbl_del_rec(it->rec, it);
}

void tbl_add_lis(String tn, String key_fld, String key)
{
  log_msg("TABLE", "SET LIS %s|%s", key_fld, key);

  /* create listener for fn_val entry list */
  fn_tbl *t = get_tbl(tn);
  fn_fld *ff;
  HASH_FIND_STR(t->fields, key_fld, ff);
  if (!ff)
    return;

  fn_lis *ll;
  HASH_FIND_STR(ff->lis, key, ll);
  if (ll)
    return;

  /* create new listener */
  log_msg("TABLE", "new lis");
  ll = malloc(sizeof(fn_lis));
  memset(ll, 0, sizeof(fn_lis));
  ll->key = strdup(key);
  ll->key_fld = ff;
  HASH_ADD_STR(ff->lis, key, ll);

  /* check if value exists and attach */
  fn_val *val;
  HASH_FIND_STR(ff->vals, key, val);
  if (val)
    ll->rec = val->rlist->rec;
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
  }
  else {
    log_msg("TABLE", "trepush");
    ventry *ent = malloc(sizeof(ventry));
    val->count = 1;
    ent->head = 1;
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
  ent->head = 0;
  ent->rec = rec;
  ent->val = v;
  ent->prev = v->rlist->prev;
  ent->next = v->rlist;
  v->rlist->prev->next = ent;
  v->rlist->prev = ent;
  rec->vlist[indx] = ent;
  rec->vals[indx] = v;
}

static void check_set_lis(fn_fld *f, String key, fn_rec *rec)
{
  /* check if field has lis. */
  if (HASH_COUNT(f->lis) < 1)
    return;

  fn_lis *ll;
  HASH_FIND_STR(f->lis, key, ll);
  if (!ll || ll->ent)
    return;

  /* if lis hasn't obtained a rec, set it now. */
  log_msg("TABLE", "SET LIS");
  ll->rec = rec;
}

static void tbl_insert(fn_tbl *t, trans_rec *trec)
{
  log_msg("TABLE", "rec");
  t->rec_count++;
  fn_rec *rec = mk_rec(t);

  for(int i = 0; i < rec->fld_count; i++) {
    void *data = trec->data[i];

    fn_fld *f;
    HASH_FIND_STR(t->fields, trec->flds[i], f);

    if (f->type == typVOID) {
      log_msg("TABLE", "void insert");
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
    ll->ent = NULL;

  free(val->key);
  val->rlist = NULL;
}

static void pop_ventry(ventry *it, fn_val *val, ventry **cur)
{
  if (it == val->rlist)
    val->rlist = it->next;

  it->next->prev = it->prev;
  it->prev->next = it->next;
  if (*cur == it)
    *cur = it->next;

  free(it);
  it = NULL;
}

static ventry* tbl_del_rec(fn_rec *rec, ventry *cur)
{
  log_msg("TABLE", "delete_rec()");
  for(int i = 0; i < rec->fld_count; i++) {
    ventry *it = rec->vlist[i];
    if (!it) {
      if (rec->vals[i]->fld == typVOID)
        free(rec->vals[i]->data);

      free(rec->vals[i]);
    }
    else {
      fn_val *val = it->val;
      it->val->count--;

      if (val->count > 1)
        pop_ventry(it, val, &cur);
      else {
        fn_fld *fld = val->fld;

        del_fldval(fld, val);
        free(rec->vals[i]);

        if (cur == it)
          cur = NULL;

        free(it);
      }
    }
  }
  free(rec->vals);
  free(rec->vlist);
  free(rec);
  return cur;
}

trans_rec* mk_trans_rec(int fld_count)
{
  trans_rec *r = malloc(sizeof(trans_rec));
  r->max = fld_count;
  r->flds = malloc(r->max * sizeof(String));
  r->data = malloc(r->max * sizeof(void*));
  r->type = malloc(r->max * sizeof(bool));
  r->count = 0;
  return r;
}

void edit_trans(trans_rec *r, String fname, String val, void *data)
{
  r->flds[r->count] = fname;
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

//TODO: cntlr has list of tables it own
// get current focus cntlr
// get cntlr's table list
// iterate list of tables
// add field to compl
void field_list(List *args)
{
  if (HASH_COUNT(FN_MASTER) < 1) return;
  fn_tbl *t = get_tbl("fm_files");
  unsigned int count = HASH_COUNT(t->fields);
  count += HASH_COUNT(t->vtfields);

  compl_new(count, COMPL_STATIC);
  fn_fld *it;
  int i = 0;
  for (it = t->fields; it != NULL; it = it->hh.next) {
    compl_set_index(i, 0, NULL, "%s", it->key);
    i++;
  }
  fn_vt_fld *vit;
  for (vit = t->vtfields; vit != NULL; vit = vit->hh.next) {
    compl_set_index(i, 0, NULL, "%s", vit->key);
    i++;
  }
}
