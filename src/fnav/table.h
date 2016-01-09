#ifndef FN_TABLE_H
#define FN_TABLE_H

#include "fnav/lib/uthash.h"
#include "fnav/tui/cntlr.h"

typedef struct fn_rec fn_rec;
typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct fn_lis fn_lis;
typedef struct tentry tentry;
typedef struct ventry ventry;

typedef enum {
  typVOID,
  typINT,
  typSTRING,
  typUINT64_T,
  //typTABLE /* record format for lists of tables */
} tFldType;

struct ventry {
  ventry *prev;
  ventry *next;
  fn_rec *rec;
  fn_val *val;
  unsigned int head;
};

struct fn_lis {
  String key;       // listening value
  fn_fld *key_fld;  // listening field
  fn_fld *fname;    // filter field
  String fval;      // filter val
  ventry *ent;      // filter val entry
  fn_rec *rec;      // record for both listening & filter
  int lnum;
  int index;
  UT_hash_handle hh;
};

void tables_init();
void tables_cleanup();
bool tbl_mk(String name);
void tbl_del(String name);
void tbl_mk_fld(String tn, String name, tFldType typ);

void tbl_add_lis(String tn, String key_fld, String key);
void commit(void **data);

ventry* fnd_val(String tn, String fname, String val);
fn_lis* fnd_lis(String tn, String key_fld, String key);
ventry* lis_set_val(fn_lis *lis, String fname);
ventry* lis_get_val(fn_lis *lis, String fname);
void lis_save(fn_lis *lis, int index, int lnum);
void* rec_fld(fn_rec *rec, String fname);
String ent_str(ventry *ent);
ventry* ent_head(ventry *ent);

void tbl_del_val(String tn, String fname, String val);

int tbl_fld_count(String tn);
int tbl_ent_count(ventry *e);

typedef struct {
  int count;
  int max;
  String *flds;
  bool *type;
  void **data;
} trans_rec;

typedef void* (*tbl_vt_cb)(fn_rec *rec, String key);
void tbl_mk_vt_fld(String tn, String name, tbl_vt_cb cb);
void* rec_vt_fld(String tn, fn_rec *rec, String fname);

trans_rec* mk_trans_rec(int fld_count);
void edit_trans(trans_rec *r, String fname, String val, void *data);
void clear_trans(trans_rec *r, int flush);

void field_list(String line);

#endif
