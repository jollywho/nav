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

void initflds(fn_fld *f, fn_rec *rec)
{
  fn_val *v = malloc(sizeof(fn_val));
  v->fld = f;
  v->listeners = NULL;
  rec->vals[rec->fld_count] = v;
  rec->vlist[rec->fld_count] = NULL;
  rec->fld_count++;
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = malloc(sizeof(fn_val)*t->count);
  rec->vlist = malloc(sizeof(fn_val)*t->count);
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

int FN_MV(ventry *e, int n)
{
  return ((n > 0) ?
    (e->next->head ? 0 : (1)) :
    (e->prev->head ? 0 : (-1)));
}

void* rec_fld(fn_rec *rec, String fname)
{
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
      log_msg("TABLE", "edit %s|%s", fname, val);
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
      rec->vlist[i] = ent;
      if (v->listeners) {
        if (v->count == 1) {
          v->listeners->ent = ent;
        }
        log_msg("TABLE", "CALL");
        v->listeners->cb(v->listeners->hndl);
      }
    }
  }
}

void tbl_del_rec(fn_rec *rec)
{
  log_msg("TABLE", "delete rec st");
  if (!rec) return;
  for(int i=0;i<rec->fld_count;i++) {
      log_msg("TABLE", "delete reer");
    ventry *it = rec->vlist[i];
    if (it && !it->head) {
      log_msg("TABLE", "delete rec inner");
      it->next->prev = it->prev;
      it->prev->next = it->next;
    }
  }
}

void tbl_del_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "delete %s,%s",fname, val);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      /* iterate entries of val. */
      ventry *it = vv->rlist->prev;
      while (!it->head) {
        tbl_del_rec(it->rec);
        it = it->prev;
      }
      if (!vv->listeners) {
        kb_delp(FNVAL, ff->vtree, vv);
      log_msg("TABLE", "delete after lis");
      }
    }
  }
}

void tbl_listener(fn_handle *hndl, buf_cb cb)
{
  log_msg("TABLE", "SET LIS %s|%s", hndl->fname, hndl->fval);
  /* create listener for fn_val entry list */
  fn_fld f = { .key = hndl->fname };
  fn_val v = { .key = hndl->fval   };
  fn_fld *ff = kb_get(FNFLD, hndl->tbl->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    ventry *ent;
    if (!vv) {
      /* if null, create the null stub. */
      vv = malloc(sizeof(fn_val));
      ent = malloc(sizeof(ventry));
      vv->key = strdup(hndl->fval);
      vv->fld = ff;
      vv->count = 0;
      ent->rec = NULL;
      ent->val = vv;
      ent->prev = ent;
      ent->next = ent;
      ent->head = 1;
      vv->rlist = ent;
      vv->listeners = NULL;
    }
    else
      ent = vv->rlist;

    if (!vv->listeners) {
      /* create new listener */
      vv->listeners = malloc(sizeof(listener));
      vv->listeners->cb = cb;
      vv->listeners->ent = ent;
      vv->listeners->hndl = hndl;
      kb_putp(FNVAL, ff->vtree, vv);
    }
    hndl->lis = vv->listeners;
    vv->listeners->cb(vv->listeners->hndl);
  }
}

// TODO: use commit bit flags
// COMMIT [ INSERT | UPDATE ]
// will attempt to update instead of insert.
// COMMIT [ INSERT | DELETE ]
// will attempt to delete before insert.

void commit(Job *job, JobArg *arg)
{
  Cntlr *c = job->caller;
  tbl_insert(c->hndl->tbl, arg->rec);
  free(arg);
  job->read_cb(job->caller);
}
