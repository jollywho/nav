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
  for(int i = 0; i < rec->fld_count; i++) { 
    if (strcmp(rec->vals[i]->fld->key, fname) == 0) {
      if (rec->vals[i]->fld->type == typSTRING) {
      return rec->vals[i]->key;
      }
      else
        return rec->vals[i]->dat;
    }
  }
  return NULL;
}

void rec_edit(fn_rec *rec, String fname, void *val)
{
  for(int i = 0; i < rec->fld_count; i++) { 
    if (strcmp(rec->vals[i]->fld->key, fname) == 0) {
      if (rec->vals[i]->fld->type == typSTRING) {
        log_msg("TABLE", "|%s|,|%s|", fname, (String)val);
        rec->vals[i]->key = strdup(val);
      }
      else {
        rec->vals[i]->key = NULL;
        rec->vals[i]->dat = val;
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

  for (int i = 0; i < rec->fld_count; i++) {
    fn_val *fv = rec->vals[i];
    if (fv->fld->type == typVOID) continue;
    fn_val *v = kb_getp(FNVAL, fv->fld->vtree, fv);
    if (!v) {
      fv->count = 1;
      fv->recs = kl_init(kl_tentry);
      kl_push(kl_tentry, fv->recs, ent);
      kb_putp(FNVAL, fv->fld->vtree, fv);
    }
    else {
      v->count++;
      kl_push(kl_tentry, v->recs, ent);
    }
  }
  // update cur to new rec
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
  free(ent);
}

void tbl_delete(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "delete %s: %s", fname, val);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      kliter_t(kl_tentry) *it;
	    for (it = kl_begin(vv->recs); it != kl_end(vv->recs); it = kl_next(it)) {
        destroy_tentry(it->data);
      }
      kb_delp(FNVAL, ff->vtree, vv);
      kl_destroy(kl_tentry, vv->recs);
    }
  }
}

void initflds(fn_fld *f, fn_rec *rec)
{
  rec->vals[rec->fld_count] = malloc(sizeof(fn_val));
  rec->vals[rec->fld_count]->fld = f;
  rec->fld_count++;
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = malloc(sizeof(fn_val)*t->count);
  rec->vals[t->count] = NULL;
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
  switch(arg->state) {
    case(REC_SEL):
      ;;
    case(REC_INS):
      tbl_insert(job->caller->tbl, arg->rec);
      ;;
    case(REC_UPD):
      ;;
    case(REC_DEL):
      ;;
  };
  job->read_cb(job->caller); // what do here?
}
