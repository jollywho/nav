#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "fnav/table.h"
#include "fnav/log.h"

KBTREE_INIT(FNVAL, fn_val_node, elem_cmp)
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
  rec->vals[rec->fld_count] = NULL;
  rec->vlist[rec->fld_count] = NULL;
  rec->fld_count++;
}

fn_rec* mk_rec(fn_tbl *t)
{
  fn_rec *rec = malloc(sizeof(fn_rec));
  rec->vals = malloc(sizeof(fn_val_node)*t->count);
  rec->vlist = malloc(sizeof(fn_val)*t->count);
  rec->fld_count = kb_size(t->fields);

  //__kb_traverse(fn_fld, t->fields, initflds, rec);
  return rec;
}

ventry* fnd_val(String tn, String fname, String val)
{
  fn_tbl *t = get_tbl(tn);
  fn_fld f = { .key = fname };
  fn_val_node v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val_node *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      return vv->node->rlist;
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

    if (strcmp(rec->vals[i]->node->fld->key, fname) == 0) {
      if (rec->vals[i]->node->fld->type == typSTRING) {
        return rec->vals[i]->key;
      }
      else {
        return rec->vals[i]->node->data;
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
    if (strcmp(rec->vals[i]->node->fld->key, fname) == 0) {
      if (rec->vals[i]->node->fld->type == typSTRING) {
        rec->vals[i]->node->key = strdup(val);
        rec->vals[i]->node->data = NULL;
      }
      else {
        rec->vals[i]->node->key = NULL;
        rec->vals[i]->node->data = val;
      }
    }
  }
}

fn_val_node* new_entry(fn_rec *rec, fn_fld *fld, void *data, int typ, int indx)
{
  fn_val_node *val = malloc(sizeof(fn_val_node));
  val->node = malloc(sizeof(fn_val));
  ventry *ent = malloc(sizeof(ventry));
  val->node->fld = fld;
  if (typ) {
    val->node->count = 1;
    val->node->data = data;
  }
  else {
    log_msg("TABLE", "trepush");
    val->node->count = 1;
    ent->head = 1;
    log_msg("TABLE", "::val:: %p", &val);
    val->node->test = 9;
    ent->next = ent;
    ent->prev = ent;
    val->node->rlist = ent;
    val->key = data;
    ent->rec = rec;
    ent->val = val;
    rec->vlist[indx] = ent;
    rec->vals[indx] = val;
    kb_putp(FNVAL, fld->vtree, val);
  }
  return val;
}

void add_entry(fn_rec *rec, fn_fld *fld, fn_val_node *v, int typ, int indx)
{
  /* attach record to an entry. */
  ventry *ent = malloc(sizeof(ventry));
  v->node->count++;
    log_msg("TABLE", "::val:: %p", &v);
  v->node->test++;
  ent->head = 0;
  ent->rec = rec;
  ent->val = v;
  ent->prev = v->node->rlist->prev;
  ent->next = v->node->rlist;
  v->node->rlist->prev->next = ent;
  v->node->rlist->prev = ent;
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

    fn_fld f = { .key = trec->flds[i] };
    fn_fld *fld = kb_get(FNFLD, t->fields, f);

    if (fld->type == typVOID) {
      log_msg("TABLE", "void insert");
      rec->vals[i] = new_entry(rec, fld, data, 1, i);
      rec->vlist[i] = NULL;
      continue;
    }

    fn_val_node v = { .key = data };
    fn_val_node *vv = kb_get(FNVAL, fld->vtree, v);

    /* create header entry. */
    if (!vv) {
      log_msg("TABLE", "new stub");
      new_entry(rec, fld, data, 0, i);
    }
    else {
      add_entry(rec, fld, vv, 0, i);
    }
    log_msg("TABLE", "ins'd res %s", v.key);

    /* check if field has lis. */
    if (kb_size(fld->ltree) > 0) {
      fn_lis l = { .key = rec->vals[i]->key };
      fn_lis *ll = kb_get(FNLIS, fld->ltree, l);
      /* field has lis so call it. */
      if (ll) {
        log_msg("TABLE", "CALL");
        /* if lis hasn't obtained a val, set it now. */
        if (!ll->f_val) {
          ll->f_val = rec->vals[i]->node;
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
    if (!it) { free(rec->vals[i]); }
    if (it) {
      log_msg("TABLE", "got entry");
      it->val->node->count--;

      if (it->val->node->count < 1 ) {
        log_msg("TABLE", "to delp %s %s", it->val->node->fld->key, it->val->key);
        fn_val_node *v = kb_getp(FNVAL, it->val->node->fld->vtree, it->val);
        log_msg("TABLE", "getp %s %s", v->node->fld->key, v->key);
        kb_del(FNVAL, v->node->fld->vtree, *v);

        fn_lis l = { .key = it->val->key   };
        fn_lis *ll = kb_get(FNLIS, it->val->node->fld->ltree, l);
        if (ll) {
          log_msg("TAB", "CLEAR ");
          ll->ent = NULL;
          ll->f_val = NULL;
          log_msg("TAB", "CLEAR ");
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
  fn_fld f = { .key = fname };
  fn_val_node v = { .key = val   };
  fn_fld *ff = kb_get(FNFLD, t->fields, f);
  if (ff) {
    fn_val_node *vv = kb_get(FNVAL, ff->vtree, v);
    if (vv) {
      /* iterate entries of val. */
      ventry *it = vv->node->rlist;
      int count = vv->node->count;
        log_msg("TAB", "DELCOUN %d", vv->node->count);
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
      ll->f_fld = NULL;
      ll->f_val = NULL;
      ll->hndl = hndl;
      ll->ent = NULL;
      ll->cb = cb;
      ll->pos = 0;
      ll->ofs = 0;
      kb_putp(FNLIS, ff->ltree, ll);
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
  free(data[1]);
}
