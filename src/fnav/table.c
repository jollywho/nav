#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "fnav/table.h"
#include "fnav/log.h"

fn_tbl* get_tbl(String t);
static void tbl_del_rec(fn_rec *rec);
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

struct fn_tbl {
  String key;
  fn_fld *fields;
  int count;
  int rec_count;
  UT_hash_handle hh;
};

fn_tbl *FN_MASTER;

void tables_init()
{
  log_msg("INIT", "table");
}

bool tbl_mk(String name)
{
  fn_tbl *t = get_tbl(name);
  if (!t) {
    log_msg("TABLE", "making table {%s} ...", name);
    fn_tbl *t = malloc(sizeof(fn_tbl));
    t->key = name;
    t->count = 0;
    t->rec_count = 0;
    t->fields = NULL;
    HASH_ADD_KEYPTR(hh, FN_MASTER, t->key, strlen(t->key), t);
    return true;
  }
  return false;
}

void tbl_del(String name)
{
  fn_tbl *t = get_tbl(name);
  fn_fld *f, *ftmp;
  HASH_ITER(hh, t->fields, f, ftmp) {
    HASH_DEL(t->fields, f);
    //TODO: del val
    //TODO: del lis
    free(f->key);
    free(f);
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
  fld->key = strdup(name);
  fld->type = typ;
  fld->vals = NULL;
  fld->lis = NULL;
  t->count++;
  HASH_ADD_KEYPTR(hh, t->fields, fld->key, strlen(fld->key), fld);
  log_msg("TABLE", "made %s", fld->key);
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = malloc(sizeof(fn_val)*t->count);
  rec->vlist = malloc(sizeof(fn_val)*t->count);
  rec->fld_count = t->count;
  return rec;
}

ventry* fnd_val(String tn, String fname, String val)
{
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fname, f);
  if (f) {
    fn_val *v;
    HASH_FIND_STR(f->vals, val, v);
    if (v) {
      return v->rlist;
    }
  }
  return NULL;
}

fn_lis* fnd_lis(String tn, String key_fld, String key)
{
  log_msg("TABLE", "fnd_lis %s %s", key_fld, key);
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, key_fld, f);
  if (f) {
    fn_lis *l;
    HASH_FIND_STR(f->lis, key, l);
    if (l) {
      return l;
    }
  }
  return NULL;
}

ventry* lis_set_val(fn_lis *lis, String fname)
{
  log_msg("TABLE", "lis_set_val");
  for(int i = 0; i < lis->rec->fld_count; i++) {
    ventry *it= lis->rec->vlist[i];
    if (it) {
      if (strcmp(it->val->fld->key, fname) == 0) {
        lis->ent = it;
        lis->fname = it->val->fld;
        lis->fval = it->val->key;
        return it;
      }
    }
  }
  return NULL;
}

int FN_MV(ventry *e, int n)
{
  return ((n > 0) ?
    (e->next->head ? 0 : (1)) :
    (e->head ? 0 : (-1)));
}

void* rec_fld(fn_rec *rec, String fname)
{
  if (!rec) return NULL;
  for(int i = 0; i < rec->fld_count; i++) {

    if (strcmp(rec->vals[i]->fld->key, fname) == 0) {
      if (rec->vals[i]->fld->type == typSTRING) {
        return rec->vals[i]->key;
      }
      else {
        return rec->vals[i]->data;
      }
    }
  }
  return NULL;
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

void tbl_del_val(String tn, String fname, String val)
{
  log_msg("TABLE", "delete_val() %s %s,%s",tn ,fname, val);
  fn_tbl *t = get_tbl(tn);
  fn_fld *f;
  HASH_FIND_STR(t->fields, fname, f);
  if (f) {
    fn_val *v;
    HASH_FIND_STR(f->vals, val, v);
    if (v) {
      /* iterate entries of val. */
      ventry *it = v->rlist;
      int count = v->count;
        log_msg("TAB", "DELCOUN %d", v->count);
      for (int i = 0; i < count; i++) {
        log_msg("TAB", "ITERHEAD %s %s %s",tn,fname,val);
        it = it->next;
        tbl_del_rec(it->rec);
      }
    }
  }
}

void tbl_add_lis(String tn, String key_fld, String key)
{
  log_msg("TABLE", "SET LIS %s|%s", key_fld, key);

  /* create listener for fn_val entry list */
  fn_tbl *t = get_tbl(tn);
  fn_fld *ff;
  HASH_FIND_STR(t->fields, key_fld, ff);
  if (ff) {
    fn_lis *ll;
    HASH_FIND_STR(ff->lis, key, ll);
    if (!ll) {
      log_msg("TABLE", "new lis");
      /* create new listener */
      ll = malloc(sizeof(fn_lis));
      ll->key = strdup(key);
      ll->key_fld = ff;
      ll->fname = NULL;
      ll->fval = NULL;
      ll->ent = NULL;
      ll->rec = NULL;
      ll->index = 0;
      ll->lnum = 0;
      HASH_ADD_STR(ff->lis, key, ll);
    }
  }
}

void commit(void **data)
{
  log_msg("TABLE", "commit");
  fn_tbl *t = get_tbl(data[0]);
  tbl_insert(t, data[1]);
  clear_trans(data[1]);
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
    val->key = data;
    ent->rec = rec;
    ent->val = val;
    rec->vlist[indx] = ent;
    rec->vals[indx] = val;
    HASH_ADD_KEYPTR(hh, fld->vals, val->key, strlen(val->key), val);
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
  log_msg("TABLE", "check set lis %s %s", f->key, key);
  /* check if field has lis. */
  if (HASH_COUNT(f->lis) > 0) {
    fn_lis *ll;
    HASH_FIND_STR(f->lis, key, ll);
    if (ll) {
      log_msg("TABLE", "SET LIS");
      /* if lis hasn't obtained a rec, set it now. */
      if (!ll->ent) {
        ll->rec = rec;
      }
    }
  }
}

static void tbl_insert(fn_tbl *t, trans_rec *trec)
{
  log_msg("TABLE", "rec");
  t->rec_count++;
  fn_rec *rec = mk_rec(t);

  for(int i = 0; i < rec->fld_count; i++) {
    log_msg("TABLE", "inserting %s %s ", trec->flds[i], (String)trec->data[i]);
    void *data = trec->data[i];

    fn_fld *f;
    HASH_FIND_STR(t->fields, trec->flds[i], f);

    if (f->type == typVOID) {
      log_msg("TABLE", "void insert");
      rec->vals[i] = new_entry(rec, f, data, 1, i);
      rec->vlist[i] = NULL;
      continue;
    }

    fn_val *v; //= { .key = data };
    HASH_FIND_STR(f->vals, data, v);

    /* create header entry. */
    if (!v) {
      log_msg("TABLE", "new stub");
      new_entry(rec, f, data, 0, i);
    }
    else {
      add_entry(rec, f, v, 0, i);
    }
    check_set_lis(f, rec->vals[i]->key, rec);
  }
}

static void tbl_del_rec(fn_rec *rec)
{
  log_msg("TABLE", "delete_rec()");
  for(int i = 0; i < rec->fld_count; i++) {
    log_msg("TABLE", "next");
    ventry *it = rec->vlist[i];
    if (!it) {
      if (rec->vals[i]->fld == typVOID) {
        free(rec->vals[i]->data);
      }
      free(rec->vals[i]);
    }
    if (it) {
      log_msg("TABLE", "got entry");
      it->val->count--;

      if (it->val->count < 1 ) {
        log_msg("TABLE", "to delp %s %s", it->val->fld->key, it->val->key);
        HASH_DEL(it->val->fld->vals, it->val);

        fn_lis *ll;
        HASH_FIND_STR(it->val->fld->lis, it->val->key, ll);
        if (ll) {
          log_msg("TAB", "CLEAR ");
          ll->ent = NULL;
        }
        free(it->val->key);
        free(rec->vals[i]);
      }
      it->next->prev = it->prev;
      it->prev->next = it->next;
      free(it);
    }
  }
  free(rec->vals);
  free(rec->vlist);
  free(rec);
}
