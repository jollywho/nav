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
  fn_rec **records;
  int count;
  int rec_count;
};

fn_tbl* tbl_mk()
{
  fn_tbl *t = malloc(sizeof(fn_tbl));
  t->count = 0;
  t->rec_count = 0;
  t->fields = kb_init(FNFLD, KB_DEFAULT_SIZE);
  t->records = malloc(sizeof(fn_rec));
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

void rec_edit(fn_rec *rec, String fname, String val)
{
  // TODO: tFldType checking on void* final arg
  //       use temp struct to store intermediaries
  //       no point running tree ops twice. do it
  //       in job->fn().
  // TODO: tFldType checking on void* final arg
  for(int i = 0; i < rec->fld_count; i++) { 
    if (strcmp(rec->fldvals[i]->fld->key, fname) == 0) {
      rec->fldvals[i]->val->key = strdup(val);
    }
  }
}

void tbl_insert(fn_tbl *t, fn_rec *rec)
{
  // TODO: significant error checking
  for (int i =0; i < rec->fld_count; i++) {
    fn_fldval *fv = rec->fldvals[i];
    fn_val *v = kb_getp(FNVAL, fv->fld->vtree, fv->val);
    if (!v) {
      fv->val->count = 1;
      fv->val->rec = malloc(sizeof(fn_rec));
      fv->val->rec[0] = rec;
      kb_putp(FNVAL, fv->fld->vtree, fv->val);
    }
    else {
      v->count++;
      v->rec = realloc(v->rec, v->count * sizeof(fn_rec));
      v->rec[v->count - 1] = rec;
    }
    t->rec_count++;
    t->records = realloc(t->records, sizeof(fn_rec)*t->rec_count);
    t->records[t->rec_count] = rec;
  }
}

void initflds(fn_fld *f, fn_rec *rec)
{
  rec->fldvals[rec->fld_count] = malloc(sizeof(fn_fldval));
  rec->fldvals[rec->fld_count]->fld = f;
  rec->fldvals[rec->fld_count]->val = malloc(sizeof(fn_val));
  rec->fld_count++;
}

/* TODO: make blank rec after fields set. */
/*       memcpy blank when new rec needed.*/
fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->fldvals = malloc(sizeof(fn_fldval)*t->count);
  rec->fldvals[t->count] = NULL;
  rec->fld_count = 0;
  __kb_traverse(fn_fld, t->fields, initflds, rec);
  return rec;
}

fn_rec** fnd_val(fn_tbl *t, String fname, String val)
{
  log_msg("TABLE", "fnd_val %s: %s", fname, val);
  fn_fld f = { .key = fname };
  fn_val v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      return vv->rec;
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
  free(arg->rec);
  job->read_cb(job->caller); // what do here?
}
