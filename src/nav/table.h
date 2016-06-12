#ifndef NV_TABLE_H
#define NV_TABLE_H

#include "nav/lib/uthash.h"
#include "nav/plugins/plugin.h"

typedef struct TblRec TblRec;
typedef struct TblVal TblVal;
typedef struct TblFld TblFld;
typedef struct TblLis TblLis;
typedef struct Tentry Tentry;
typedef struct Ventry Ventry;
typedef struct nv_vt_fld nv_vt_fld;

#define TYP_VOID     0
#define TYP_STR      1
#define TYP_INT      2
#define SRT_DFLT     1
#define SRT_STR     16
#define SRT_NUM     32
#define SRT_TIME    64
#define SRT_TYPE   128
#define SRT_DIR    256

struct Ventry {
  Ventry *prev;
  Ventry *next;
  TblRec *rec;
  TblVal *val;
};

struct TblLis {
  char *key;       // listening value
  TblFld *key_fld;  // listening field
  char *fname;     // filter field
  char *fval;      // filter val
  TblRec *rec;      // record for both listening & filter
  int lnum;
  int index;
  UT_hash_handle hh;
};

void tables_init();
void tables_cleanup();
bool tbl_mk(const char *);
void tbl_del(const char *);
void tbl_mk_fld(const char *, const char *, int);

Table* get_tbl(const char *tn);
void tbl_add_lis(const char *, const char *, const char *);
void tbl_del_fld_lis(TblFld *);
void commit(void **data);

Ventry* fnd_val(const char *, const char *, const char *);
TblLis* fnd_lis(const char *, const char *, const char *);
Ventry* lis_get_val(TblLis *lis, const char *);
void lis_save(TblLis *lis, int index, int lnum, const char *);
void* rec_fld(TblRec *rec, const char *);
char* ent_str(Ventry *ent);
Ventry* ent_head(Ventry *ent);
Ventry* ent_rec(TblRec *rec, const char *);
TblRec* tbl_iter(TblRec *next);
int fld_type(const char *, const char *);

void tbl_del_val(const char *, const char *, const char *);

int tbl_fld_count(const char *);
int tbl_ent_count(Ventry *e);

typedef struct {
  int count;
  int max;
  char **flds;
  int *type;
  void **data;
} trans_rec;

void tbl_mk_vt_fld(const char *, const char *, int);

trans_rec* mk_trans_rec(int fld_count);
void edit_trans(trans_rec *r, char *, char *, void *data);
void clear_trans(trans_rec *r, int flush);

void field_list(List *args);
void record_list(const char *tn, char *f1, char *f2);

#endif
