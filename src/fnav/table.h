#ifndef FN_CORE_TABLE_H
#define FN_CORE_TABLE_H

#include "fnav/lib/uthash.h"
#include "fnav/rpc.h"

#define elem_cmp(a, b) (strcmp((a).key, (b).key))

typedef struct fn_rec fn_rec;
typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct tentry tentry;
typedef struct ventry ventry;

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
  UT_hash_handle hh;
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

struct fn_lis {
  String key;    // value being listened
  fn_fld *f_fld; // filter field
  fn_val *f_val; // filter value
  fn_handle *hndl;
  ventry *ent;
  buf_cb cb;
  int pos;
  int ofs;
  UT_hash_handle hh;
};

void tables_init();
void tbl_mk(String name);
fn_rec* mk_rec(fn_tbl *t);
void rec_edit(fn_rec *rec, String fname, void *val);
void tbl_mk_fld(String tn, String name, tFldType typ);
void tbl_del_val(String tn, String fname, String val);
void tbl_listener(fn_handle *hndl, buf_cb cb);
ventry* fnd_val(String tn, String fname, String val);
void commit(void **data);

int tbl_count(String tn);
void* rec_fld(fn_rec *rec, String fname);

int FN_MV(ventry *e, int n);

#endif
