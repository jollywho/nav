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
  tentry *cur_entry;
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
  t->records->prev = NULL;
  t->records->next = NULL;
  t->cur_entry = t->records;
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

void rec_edit(fn_rec *rec, String fname, void *val)
{
  FN_KL_ITERBLK(kl_val, rec->vals) {
    if (strcmp(it->data->fld->key, fname) == 0) {
      if (it->data->fld->type == typSTRING) {
        log_msg("TABLE", "|%s|,|%s|", fname, (String)val);
        it->data->key = strdup(val);
      }
      else {
        it->data->key = NULL;
        it->data->dat = val;
      }
    }
  }
}

void tbl_insert(fn_tbl *t, fn_rec *rec)
{
  t->rec_count++;
  tentry *ent = malloc(sizeof(tentry));
  ent->rec = rec;
  ent->prev = t->cur_entry;
  ent->next = NULL;

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
  /* update cur to new rec */
  t->cur_entry->next = ent;
  t->cur_entry = ent;
}

void destroy_tentry(tentry *ent)
{
  log_msg("TABLE", "del ent");
  if (ent->prev && ent->next) {
    ent->prev->next = ent->next;
    ent->next->prev = ent->prev;
  }
  kl_destroy(kl_val, ent->rec->vals);
  free(ent->rec);
}

void tbl_del_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "delete %s: %s", fname, val);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      /* iterate record ptrs. */
      FN_KL_ITERBLK(kl_tentry, vv->recs) {
        destroy_tentry(it->data);
      }
      kb_delp(FNVAL, ff->vtree, vv);
      /* destroy entire list of record ptrs. */
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

void commit(Job *job, JobArg *arg)
{
  log_msg("TABLE", "commit");
  tbl_insert(job->caller->hndl->tbl, arg->rec);
  job->read_cb(job->caller);
  job->updt_cb(job, arg);
}
