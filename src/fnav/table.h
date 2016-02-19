#ifndef FN_TABLE_H
#define FN_TABLE_H

#include "fnav/lib/uthash.h"
#include "fnav/plugins/plugin.h"

typedef struct fn_rec fn_rec;
typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct fn_vt_fld fn_vt_fld;
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
};

struct fn_lis {
  char *key;       // listening value
  fn_fld *key_fld;  // listening field
  char *fname;     // filter field
  char *fval;      // filter val
  fn_rec *rec;      // record for both listening & filter
  int lnum;
  int index;
  UT_hash_handle hh;
};

void tables_init();
void tables_cleanup();
bool tbl_mk(const char *);
void tbl_del(const char *);
void tbl_mk_fld(const char *, const char *, tFldType);

fn_tbl* get_tbl(const char *tn);
void tbl_add_lis(const char *, const char *, const char *);
void commit(void **data);

ventry* fnd_val(const char *, const char *, const char *);
fn_lis* fnd_lis(const char *, const char *, const char *);
ventry* lis_get_val(fn_lis *lis, const char *);
void lis_save(fn_lis *lis, int index, int lnum);
void* rec_fld(fn_rec *rec, const char *);
char* ent_str(ventry *ent);
ventry* ent_head(ventry *ent);
ventry* ent_rec(fn_rec *rec, const char *);

void tbl_del_val(const char *, const char *, const char *);

int tbl_fld_count(const char *);
int tbl_ent_count(ventry *e);

typedef void* (*tbl_vt_cb)(fn_rec *rec, const char *key);

typedef struct {
  int count;
  int max;
  char **flds;
  int *type;
  void **data;
} trans_rec;

void tbl_mk_vt_fld(const char *, const char *, tbl_vt_cb cb);
void* rec_vt_fld(const char *, fn_rec *rec, const char *);

trans_rec* mk_trans_rec(int fld_count);
void edit_trans(trans_rec *r, char *, char *, void *data);
void clear_trans(trans_rec *r, int flush);

void field_list(List *args);

#endif
