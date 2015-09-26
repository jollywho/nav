#ifndef FN_CORE_TABLE_H
#define FN_CORE_TABLE_H

#include "fnav/lib/kbtree.h"
#include "fnav/rpc.h"

#define elem_cmp(a, b) (strcmp((a).key, (b).key))

typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct tentry tentry;
typedef struct ventry ventry;

#define _NOP(x)

typedef enum {
  typVOID,
  typINT,
  typSTRING,
  typUINT64_T,
  //typTABLE /* record format for lists of tables */
} tFldType;

struct fn_val {
  String key;
  void *data;
  ventry *rlist;
  fn_fld *fld;
  int count;
  listener *listeners;
};

struct fn_rec {
  fn_val **vals;
  ventry **vlist;
  int fld_count;
};

struct ventry {
  ventry *prev;
  ventry *next;
  fn_rec *rec;
  fn_val *val;
  unsigned int head;
};

struct listener {
  fn_handle *hndl;
  buf_cb cb;
  ventry *ent;
};

fn_tbl* tbl_mk();
fn_rec* mk_rec(fn_tbl *t);
void rec_edit(fn_rec *rec, String fname, void *val);
void tbl_mk_fld(fn_tbl *t, String name, tFldType typ);
void tbl_del_val(fn_tbl *t, String fname, String val);
void tbl_listener(fn_handle *hndl, buf_cb cb);
ventry* fnd_val(fn_tbl *t, String fname, String val);
void commit(Job *job, JobArg *arg);

int tbl_count(fn_tbl *t);
void* rec_fld(fn_rec *rec, String fname);

int FN_MV(ventry *e, int n);

#endif
