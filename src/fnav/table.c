#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "fnav/table.h"
#include "fnav/log.h"

KBTREE_INIT(FNVAL, fn_val, elem_cmp)
KBTREE_INIT(FNLIS, fn_lis, elem_cmp)

struct fn_fld {
  String key;
  tFldType type;
  kbtree_t(FNVAL) *vtree;
  kbtree_t(FNLIS) *ltree;
};

KBTREE_INIT(FNFLD, fn_fld, elem_cmp)

struct fn_tbl {
  String key;
  kbtree_t(FNFLD) *fields;
  int count;
  int rec_count;
};
KBTREE_INIT(FNTBL, fn_tbl, elem_cmp)

typedef struct {
  String key;
  kbtree_t(FNTBL) *name;
} tbl_list;

tbl_list FN_MASTER;

void tables_init()
{
  log_msg("INIT", "table");
  FN_MASTER.name = kb_init(FNTBL, KB_DEFAULT_SIZE);
}

void tbl_mk(String name)
{
  log_msg("TABLE", "making table {%s} ...", name);
  fn_tbl *t = malloc(sizeof(fn_tbl));
  t->key = name;
  t->count = 0;
  t->rec_count = 0;
  t->fields = kb_init(FNFLD, KB_DEFAULT_SIZE);
  kb_putp(FNTBL, FN_MASTER.name, t);
}

fn_tbl* get_tbl(String t)
{
  fn_tbl tbl = { .key = t };
  fn_tbl *ret = kb_get(FNTBL, FN_MASTER.name, tbl);
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
  fld->vtree = kb_init(FNVAL, KB_DEFAULT_SIZE);
  fld->ltree = kb_init(FNLIS, KB_DEFAULT_SIZE);
  log_msg("TABLE", "made %s", fld->key);
  t->count++;
  kb_putp(FNFLD, t->fields, fld);
}

void initflds(fn_fld *f, fn_rec *rec)
{
  fn_val *v = malloc(sizeof(fn_val));
  v->fld = f;
  rec->vals[rec->fld_count] = v;
  rec->vlist[rec->fld_count] = NULL;
  rec->fld_count++;
}

fn_rec* mk_rec(String tn)
{
  fn_tbl *t = get_tbl(tn);
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = malloc(sizeof(fn_val)*t->count);
  rec->vlist = malloc(sizeof(fn_val)*t->count);
  rec->fld_count = 0;
  __kb_traverse(fn_fld, t->fields, initflds, rec);
  return rec;
}

ventry* fnd_val(String tn, String fname, String val)
{
  fn_tbl *t = get_tbl(tn);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      return vv->rlist->next;
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
  return (get_tbl(tn)->count);
}

void rec_edit(fn_rec *rec, String fname, void *val)
{
  for(int i=0;i<rec->fld_count;i++) {
    if (strcmp(rec->vals[i]->fld->key, fname) == 0) {
      if (rec->vals[i]->fld->type == typSTRING) {
        rec->vals[i]->key = strdup(val);
        rec->vals[i]->data = NULL;
      }
      else {
        rec->vals[i]->key = NULL;
        rec->vals[i]->data = val;
      }
    }
  }
}

void tbl_insert(fn_tbl *t, fn_rec *rec)
{
  log_msg("TABLE", "rec");
  t->rec_count++;

  for(int i = 0; i < rec->fld_count; i++) {
    fn_val *v = rec->vals[i];
    if (v->fld->type == typVOID) {
      continue;
    }

    fn_fld *f = kb_getp(FNFLD, t->fields, v->fld);
    fn_val *vv = kb_getp(FNVAL, f->vtree, v);

    ventry *ent = malloc(sizeof(ventry));
    /* create header entry. */
    if (!vv) {
      log_msg("TABLE", "new stub");
      ent->next = ent;
      ent->prev = ent;
      v->rlist = ent;
      // TODO: rec_fld is returning a value with count 0 which breaks stat.
      // there is a false assumption that the value incremented below is
      // linked to the value inserted into the tree.
      // having fnd_val return the next entry works somewhat.
      // maybe related, delete still crashes. small sized tests show
      // the header remaining intact after delete.
      v->count = 0;
      ent->head = 1;
      log_msg("TABLE", "trepush");
      kb_putp(FNVAL, f->vtree, v);
      log_msg("TABLE", "trepushed");
    }
    else {
      ent->head = 0;
      // TODO: loose fnval not being used should be cleared.
    }
    /* reattach variable for less code. */
    if (!vv) {
      log_msg("TABLE", "treeget");
      vv = kb_getp(FNVAL, f->vtree, v);
      log_msg("TABLE", "tregot");
    }
    /* attach record to an entry. */
    log_msg("TABLE", "value %s %s %s", t->key, f->key, vv->key);
    vv->count++;
    ent->rec = rec;
    ent->val = vv;
    ent->prev = vv->rlist->prev;
    ent->next = vv->rlist;
    vv->rlist->prev->next = ent;
    vv->rlist->prev = ent;
    rec->vlist[i] = ent;

    /* check if field has lis. */
    if (kb_size(f->ltree) > 0) {
      fn_lis l = { .key = vv->key };
      fn_lis *ll = kb_get(FNLIS, f->ltree, l);
      /* field has lis so call it. */
      if (ll) {
        log_msg("TABLE", "CALL");
        /* if lis hasn't obtained a val, set it now. */
        if (!ll->f_val) {
          ll->f_val = vv;
          ll->ent = ent;
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
  if (!rec) return;
  for(int i = 0; i < rec->fld_count; i++) {
    log_msg("TABLE", "next");
    ventry *it = rec->vlist[i];
    log_msg("TABLE", "next got");
    if (it) {
      log_msg("TABLE", "it next got");
      it->val->count--;
      it->next->prev = it->prev;
      it->prev->next = it->next;

      if (it->val->count < 1 ) {
        log_msg("TABLE", "deletep %s %s", it->val->fld->key, it->val->key);
        kb_delp(FNVAL, it->val->fld->vtree, it->val);
        log_msg("TABLE", "dele %s %s", it->val->fld->key, it->val->key);
      }
    }
  }
// TODO: cannot free record or its vals with this strategy
//
//  free(it->val->data);
//  free(rec->vals);
//  free(rec->vlist);
//  free(rec);
}

void tbl_del_val(String tn, String fname, String val)
{
  log_msg("TABLE", "delete_val() %s %s,%s",tn ,fname, val);
  fn_tbl *t = get_tbl(tn);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      /* iterate entries of val. */
      ventry *it = vv->rlist;
      int count = vv->count;
      for (int i = 0; i < count; i++) {
        log_msg("TAB", "ITERHEAD %s %s %s",tn,fname,val);
        it = it->next;
        tbl_del_rec(it->rec);
      }
      fn_lis l = { .key = val   };
      fn_lis *ll = kb_get(FNLIS, ff->ltree, l);
      if (ll) {
        log_msg("TAB", "CLEAR ");
        ll->f_val = NULL;
        ll->ent = NULL;
      }
    }
  }
}

void tbl_listener(fn_handle *hndl, buf_cb cb)
{
  log_msg("TABLE", "SET LIS %s|%s", hndl->fname, hndl->fval);

  /* create listener for fn_val entry list */
  fn_tbl *t = get_tbl(hndl->tname);
  fn_fld f = { .key = hndl->fname };
  fn_lis l = { .key = hndl->fval   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_lis *ll = kb_get(FNLIS, ff->ltree, l);
    if (!ll) {
      log_msg("TABLE", "new lis");
      /* create new listener */
      ll = malloc(sizeof(fn_lis));
      ll->key = strdup(hndl->fval);
      ll->f_fld = ff;
      ll->f_val = NULL;
      ll->hndl = hndl;
      ll->ent = NULL;
      ll->cb = cb;
      ll->pos = 0;
      ll->ofs = 0;
      kb_putp(FNLIS, ff->ltree, ll);
    }

    hndl->lis = ll;
    ll->cb(ll->hndl);
  }
}

void commit(void **data)
{
  log_msg("TABLE", "commit");
  fn_tbl *t = get_tbl(data[0]);
  tbl_insert(t, data[1]);
}
