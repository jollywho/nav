#ifndef FN_CORE_TABLE_H
#define FN_CORE_TABLE_H

#include "fnav/rpc.h"

typedef enum {
  TBL_NUL,
  TBL_VOID, // for data ptr
  TBL_INT,
  TBL_STRING,
  TBL_UINT64_T,
} TBL_TYPE;

typedef struct {
  String name;
  TBL_TYPE type;
  void *value;
} TBL_field;

typedef struct {
  int flds_num;
  TBL_field **fields;
} TBL_rec;

struct TBL_handle {
  TBL_rec *tbl_sig;
  TBL_rec *ret;
};

TBL_rec* table_gen_rec(TBL_handle *tbl_handle);
void table_add(Job *job, void **args);

#endif
