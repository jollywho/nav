#ifndef FN_RPC_H
#define FN_RPC_H

#include "fnav/tui/cntlr.h"

typedef struct {
  int count;
  int max;
  String *flds;
  void **data;
  int flag;
} trans_rec;

void rpc_temp_init();
void rpc_handle(String cmdstr);
void rpc_key_handle(int key);
void rpc_draw_handle();
void rpc_handle_for(String cntlr_name, String cmdstr);
trans_rec* mk_trans_rec(int fld_count);
void edit_trans(trans_rec *r, String fname, String val, void *data);
void clear_trans(trans_rec *r);

#endif
