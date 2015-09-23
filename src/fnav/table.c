#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "fnav/table.h"
#include "fnav/log.h"

KBTREE_INIT(FNVAL, fn_val, elem_cmp)
struct fn_fld {
  String key;
  tFldType type;
  kbtree_t(FNVAL) *vtree;
};

KBTREE_INIT(FNFLD, fn_fld, elem_cmp)

struct fn_tbl {
  kbtree_t(FNFLD) *fields;
  int count;
  int rec_count;
};

fn_tbl* tbl_mk()
{
  fn_tbl *t = malloc(sizeof(fn_tbl));
  t->count = 0;
  t->rec_count = 0;
  t->fields = kb_init(FNFLD, KB_DEFAULT_SIZE);
  return t;
}

void tbl_mk_fld(fn_tbl *t, String name, tFldType typ)
{
  log_msg("TABLE", "mkfld");
  fn_fld *fld = malloc(sizeof(fn_fld));
  fld->key = strdup(name);
  fld->type = typ;
  fld->vtree = kb_init(FNVAL, KB_DEFAULT_SIZE);
  log_msg("TABLE", "made %s", fld->key);
  t->count++;
  kb_putp(FNFLD, t->fields, fld);
}

void* rec_fld(fn_rec *rec, String fname)
{
  log_msg("TABLE", "recfld");
  if (!rec) return NULL;
  for(int i=0;i<rec->fld_count;i++) {
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

int tbl_count(fn_tbl *t)
{
  return t->rec_count;
}

void rec_edit(fn_rec *rec, String fname, void *val)
{
  for(int i=0;i<rec->fld_count;i++) {
    if (strcmp(rec->vals[i]->fld->key, fname) == 0) {
      if (rec->vals[i]->fld->type == typSTRING) {
        rec->vals[i]->key = strdup(val);
      }
      else
        rec->vals[i]->key = NULL;
      rec->vals[i]->data = val;
    }
  }
}

void tbl_insert(fn_tbl *t, fn_rec *rec)
{
  log_msg("TABLE", "$rec$");
  t->rec_count++;

  for(int i=0;i<rec->fld_count;i++) {
    fn_val *fv = rec->vals[i];
    if (fv->fld->type == typVOID) {
      continue;
    }

    fn_val *v = kb_getp(FNVAL, fv->fld->vtree, fv);
    /* new value so create new tree node and rec list */
    if (!v) {
      fv->count = 1;
      ventry *ent = malloc(sizeof(ventry));
      ent->rec = rec;
      ent->val = fv;
      ent->prev = ent;
      ent->next = ent;
      ent->head = 1;
      fv->rlist = ent;
      kb_putp(FNVAL, fv->fld->vtree, fv);
    }
    /* value already exists */
    else {
      v->count++;
      ventry *ent = malloc(sizeof(ventry));
      ent->rec = rec;
      ent->val = fv;
      ent->prev = v->rlist->prev;
      ent->next = v->rlist;
      ent->head = 0;
      v->rlist->prev->next = ent;
      v->rlist->prev = ent;
      if (v->count == 1) {
        v->listeners->entry = ent;
      }
      if (v->listeners) {
        v->listeners->cb(v->listeners);
      }
    }
  }
}

void tbl_del_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "delete %s,%s",fname, val);
  return;
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      /* iterate listeners. */
      if (!vv->rlist->next) return;
      ventry *it = vv->rlist->next;
      for (int i = 1; i < vv->count; i++) {
        it = it->next;
        t->rec_count--;
      }
      if (!vv->listeners) {
        kb_delp(FNVAL, ff->vtree, vv);
      }
    }
  }
}

void initflds(fn_fld *f, fn_rec *rec)
{
  log_msg("TABLE", "initfld %s", f->key);
  fn_val *v = malloc(sizeof(fn_val));
  v->fld = f;
  v->listeners = NULL;
  rec->vals[rec->fld_count] = v;
  rec->fld_count++;
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  log_msg("TABLE", "malloc rec amt %d", t->count);
  rec->vals = malloc(sizeof(fn_val)*t->count);
  rec->fld_count = 0;
  __kb_traverse(fn_fld, t->fields, initflds, rec);
  return rec;
}

ventry* fnd_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "fnd_val %s: %s", fname, val);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      if (vv->count > 0)
        return vv->rlist->next;
    }
  }
  return NULL;
}

void tbl_listener(fn_tbl *t, listener *lis, String fname, String val)
{
  log_msg("TABLE", "listen to %s %s", fname, val);
  /* set listener to value's linked list of tentries. */
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      log_msg("TABLE", "val exists %s", val);
      vv->listeners = lis;
      lis->entry = vv->rlist->next;
      lis->cb(lis);
    }
    else {
      /* if null, create the stub. */
      log_msg("TABLE", "null create");
      fn_val *vo = malloc(sizeof(fn_val));
      ventry *ent = malloc(sizeof(ventry));
      vo->key = val;
      vo->fld = ff;
      vo->count = 0;
      ent->rec = NULL;
      ent->val = vo;
      ent->prev = ent;
      ent->next = ent;
      ent->head = 1;
      vo->rlist = ent;
      vo->listeners = lis;
      lis->entry = ent;
      kb_putp(FNVAL, ff->vtree, vo);
    }
  }
}

void commit(Job *job, JobArg *arg)
{
  Cntlr *c = job->caller;
  tbl_insert(c->hndl->tbl, arg->rec);
  free(arg);
  job->read_cb(job->caller);
}
