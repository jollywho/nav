#ifndef FN_CORE_TABLE_H
#define FN_CORE_TABLE_H

#include "fnav/lib/kbtree.h"
#include "fnav/rpc.h"

typedef enum {
  typVOID, // for struct ptr
  typINT,
  typSTRING,
  typUINT64_T,
  //typTABLE /* record format for lists of tables */
} tFldType;

typedef enum {
  REC_SEL,
  REC_INS,
  REC_UPD,
  REC_DEL,
} REC_STATE;

#define elem_cmp(a, b) (strcmp((a).key, (b).key))

typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct fn_fldval fn_fldval;
typedef struct fn_rec fn_rec;

struct fn_val {
  String key;
  fn_rec *rec;
};

struct fn_fldval {
  fn_fld *fld;
  fn_val *val;
};

struct fn_rec {
  fn_fldval **fldvals;
  int count;
};

typedef struct {
  fn_rec *rec;
  REC_STATE state;
} JobArg;

fn_tbl* tbl_mk();
fn_rec* mk_rec(fn_tbl *t);
void rec_edit(fn_rec *rec, String fname, String val);
void tbl_mk_fld(fn_tbl *t, String name, tFldType type);
fn_rec* fnd_val(fn_tbl *t, String fname, String val);
void commit(Job *job, JobArg *arg);

#endif
