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

void tbl_mk_fld(fn_tbl *t, String name, tFldType type)
{
  log_msg("TABLE", "mkfld");
  fn_fld *fld = malloc(sizeof(fn_fld));
  fld->key = strdup(name);
  fld->type = type;
  fld->vtree = kb_init(FNVAL, KB_DEFAULT_SIZE);
  t->count++;
  kb_putp(FNFLD, t->fields, fld);
}

void* rec_fld(fn_rec *rec, String fname)
{
  if (!rec) return NULL;
  FN_KL_ITERBLK(kl_val, rec->vals) {
    if (strcmp(it->data->fld->key, fname) == 0) {
      if (it->data->fld->type == typSTRING) {
        return it->data->key;
      }
      else
        return it->data->dat;
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
  FN_KL_ITERBLK(kl_val, rec->vals) {
    if (strcmp(it->data->fld->key, fname) == 0) {
      if (it->data->fld->type == typSTRING) {
        it->data->key = strdup(val);
      }
      else {
        it->data->key = NULL;
        it->data->dat = val;
      }
    }
  }
}

void alert_listeners(klist_t(kl_listen) *lst)
{
  FN_KL_ITERBLK(kl_listen, lst) {
    it->data->cb(it->data->buf);
  }
}

void tbl_insert(fn_tbl *t, fn_rec *rec)
{
  t->rec_count++;

  FN_KL_ITERBLK(kl_val, rec->vals) {
    fn_val *fv = it->data;
    if (fv->fld->type == typVOID) continue;

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
      if (v->count == 1) {
        v->listeners->entry = ent;
      }
      v->rlist->prev->next = ent;
      v->rlist->prev = ent;
      v->listeners->cb(v->listeners);
    }
  }
}

void tbl_del_val(fn_tbl *t, String fname, String val)
{
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
        free(it->prev->rec);
        free(it);
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
  fn_val *v = malloc(sizeof(fn_val));
  v->fld = f;
  rec->fld_count++;
  kl_push(kl_val, rec->vals, v);
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = kl_init(kl_val);
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
        return vv->rlist;
    }
  }
  return NULL;
}

void tbl_listener(fn_tbl *t, listener *lis, String fname, String val)
{
  log_msg("TABLE", "listen");
  /* set listener to value's linked list of tentries. */
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      vv->listeners = lis;
    }
    else {
      /* if null, create the stub. */
      fn_val *v = malloc(sizeof(fn_val));
      ventry *ent = malloc(sizeof(ventry));
      v->key = val;
      // TODO: ventry has to watch values. returns records
      v->fld = ff;
      v->count = 0;
      ent->val = v;
      ent->prev = ent;
      ent->next = ent;
      ent->head = 1;
      v->rlist = ent;
      v->listeners = lis;
      lis->entry = ent;
      kb_putp(FNVAL, ff->vtree, v);
    }
  }
}

void commit(Job *job, JobArg *arg)
{
  Cntlr *c = job->caller;
  tbl_insert(c->hndl->tbl, arg->rec);
  job->read_cb(job->caller);
}
