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
  tentry *records;
  int count;
  int rec_count;
};

fn_tbl* tbl_mk()
{
  fn_tbl *t = malloc(sizeof(fn_tbl));
  t->count = 0;
  t->rec_count = 0;
  t->fields = kb_init(FNFLD, KB_DEFAULT_SIZE);
  t->records = malloc(sizeof(tentry));
  t->records->prev = t->records;
  t->records->next = t->records;
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
  tentry *ent = malloc(sizeof(tentry));
  ent->rec = rec;
  ent->prev = t->records->prev;
  ent->next = t->records;
  rec->ent = ent;
  ent->listener = NULL;

  FN_KL_ITERBLK(kl_val, rec->vals) {
    fn_val *fv = it->data;
    if (fv->fld->type == typVOID) continue;
    fn_val *v = kb_getp(FNVAL, fv->fld->vtree, fv);

    /* new value so create new tree node and rec list */
    if (!v) {
      fv->count = 1;
      fv->recs = kl_init(kl_tentry);
      kl_push(kl_tentry, fv->recs, ent);
      kb_putp(FNVAL, fv->fld->vtree, fv);
    }
    /* value exists so just append */
    else {
      v->count++;
      kl_push(kl_tentry, v->recs, ent);
    }
  }
  t->records->prev->next = ent;
  t->records->prev = ent;

  // bump listener to first real record
  if (t->rec_count == 1) {
    ent->listener = t->records->listener;
    ent->listener->cb(ent->listener->buf, ent);
  }
}

void destroy_tentry(fn_tbl *t, tentry *ent)
{
  ent->next->prev = ent->prev;
  ent->prev->next = ent->next;
  if (ent->listener) {
    ent->prev->listener = ent->listener;
    ent->listener->cb(ent->listener->buf, ent->prev);
  }
  t->rec_count--;
  kl_destroy(kl_val, ent->rec->vals);
  free(ent->rec);
  free(ent);
}

void tbl_del_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "del");
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      /* iterate record ptrs. */
      FN_KL_ITERBLK(kl_tentry, vv->recs) {
        destroy_tentry(t, it->data);
      }
      kb_delp(FNVAL, ff->vtree, vv);
      /* destroy entire list of tentry ptrs. */
      kl_destroy(kl_tentry, vv->recs);
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

klist_t(kl_tentry)* fnd_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "fnd_val %s: %s", fname, val);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      return vv->recs;
    }
  }
  return NULL;
}

tentry* tbl_listen(fn_tbl *t, fn_buf *buf, buf_cb cb)
{
  log_msg("TABLE", "listen");
  listener *lis = malloc(sizeof(listener));
  lis->buf = buf;
  lis->cb = cb;
  t->records->listener = lis;
  return t->records;
}

void commit(Job *job, JobArg *arg)
{
  Cntlr *c = job->caller;
  tbl_insert(c->hndl->tbl, arg->rec);
  job->read_cb(job->caller);
}
