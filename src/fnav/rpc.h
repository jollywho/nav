#ifndef FN_RPC_H
#define FN_RPC_H

#include "fnav/tui/cntlr.h"

typedef void(*cntlr_cb)();
typedef void(*buf_cb)();

typedef struct {
  void *caller;
  fn_handle *hndl;
  cntlr_cb read_cb;
  buf_cb   updt_cb;
  buf_cb   draw_cb;
} Job;

typedef struct {
  int count;
  int max;
  String *flds;
  void **data;
} trans_rec;

typedef struct {
  void(*fn)();
  trans_rec *trec;
  String tname;
} JobArg;

void rpc_temp_init();
void rpc_handle(String cmdstr);
void rpc_key_handle(int key);
void rpc_draw_handle();
void rpc_handle_for(String cntlr_name, String cmdstr);
trans_rec* mk_trans_rec(int fld_count);
void edit_trans(trans_rec *r, String fname, String val, void *data);
void clear_trans(trans_rec *r);

#endif
