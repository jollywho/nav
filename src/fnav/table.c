#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "fnav/table.h"
#include "fnav/log.h"

fn_tbl* get_tbl(String t);

struct fn_val {
  String key;
  void *data;
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

void tbl_mk(String name)
{
  log_msg("TABLE", "making table {%s} ...", name);
  fn_tbl *t = malloc(sizeof(fn_tbl));
  t->key = name;
  t->count = 0;
  t->rec_count = 0;
  t->fields = NULL;
  HASH_ADD_KEYPTR(hh, FN_MASTER, t->key, strlen(t->key), t);
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

void initflds(fn_fld *f, fn_rec *rec)
{
  rec->vals[rec->fld_count] = NULL;
  rec->vlist[rec->fld_count] = NULL;
  rec->fld_count++;
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = malloc(sizeof(fn_val)*t->count);
  rec->vlist = malloc(sizeof(fn_val)*t->count);
  rec->fld_count = t->count;

  //__kb_traverse(fn_fld, t->fields, initflds, rec);
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

int tbl_count(String tn)
{
  return get_tbl(tn)->count;
}

int ent_count(ventry *e)
{
  return e->val->count;
}

fn_val* new_entry(fn_rec *rec, fn_fld *fld, void *data, int typ, int indx)
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

void add_entry(fn_rec *rec, fn_fld *fld, fn_val *v, int typ, int indx)
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

void tbl_insert(fn_tbl *t, trans_rec *trec)
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

    /* check if field has lis. */
    if (HASH_COUNT(f->lis) > 0) {
      fn_lis *ll;
      HASH_FIND_STR(f->lis, rec->vals[i]->key, ll);
      /* field has lis so call it. */
      if (ll) {
        log_msg("TABLE", "CALL");
        /* if lis hasn't obtained a val, set it now. */
        if (!ll->f_val) {
          ll->f_val = rec->vals[i];
          ll->ent = rec->vlist[i];
        }
        ll->hndl->lis = ll;
        ll->cb(ll->hndl);
      }
    }
  }
}

void tbl_del_rec(fn_rec *rec)
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
          ll->f_val = NULL;
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

void tbl_listener(fn_handle *hndl, buf_cb cb)
{
  log_msg("TABLE", "SET LIS %s|%s", hndl->fname, hndl->fval);

  /* create listener for fn_val entry list */
  fn_tbl *t = get_tbl(hndl->tname);
  fn_fld *ff;
  HASH_FIND_STR(t->fields, hndl->fname, ff);
  if (ff) {
    fn_lis *ll;
    HASH_FIND_STR(ff->lis, hndl->fval, ll);
    if (!ll) {
      log_msg("TABLE", "new lis");
      /* create new listener */
      ll = malloc(sizeof(fn_lis));
      ll->key = strdup(hndl->fval);
      ll->f_fld = NULL;
      ll->f_val = NULL;
      ll->hndl = hndl;
      ll->ent = NULL;
      ll->cb = cb;
      ll->pos = 0;
      ll->ofs = 0;
      HASH_ADD_STR(ff->lis, key, ll);
    }
    ll->hndl->lis = ll;
    ll->cb(ll->hndl);
  }
}

void commit(void **data)
{
  log_msg("TABLE", "commit");
  fn_tbl *t = get_tbl(data[0]);
  tbl_insert(t, data[1]);
  clear_trans(data[1]);
}
