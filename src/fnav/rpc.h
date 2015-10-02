#ifndef FN_CORE_RPC_H
#define FN_CORE_RPC_H

#include <uv.h>

#include "fnav/lib/queue.h"
#include "fnav/cntlr.h"

typedef void(*cntlr_cb)();
typedef void(*buf_cb)();
typedef struct fn_rec fn_rec;

typedef struct {
  void *caller;
  fn_handle *hndl;
  cntlr_cb read_cb;
  buf_cb   updt_cb;
  buf_cb   draw_cb;
} Job;

typedef struct {
  void(*fn)();
  fn_rec *rec;
  String tname;
} JobArg;

void rpc_temp_init();
void rpc_handle(String cmdstr);
void rpc_key_handle(int key);
void rpc_draw_handle();
void rpc_handle_for(String cntlr_name, String cmdstr);

#endif
