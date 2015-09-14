#ifndef FN_CORE_RPC_H
#define FN_CORE_RPC_H

#include <uv.h>

#include "fnav/lib/queue.h"
#include "fnav/cntlr.h"

//  +----<-----+---->-----+----<-----+
//  |  Event   |          |   Spin   |
//  v          ^   RPC    v          ^
//  |  Loop    |          |   Loop   |
//  +---->-----+----<-----+---->-----+
//
//  +----------+----------+----------+
//  | Channels |  Input   |   Jobs   |
//  +----------+----------+----------+

typedef uv_loop_t Loop;
typedef void(*cntlr_cb)();

typedef struct {
  Cntlr *caller;
  cntlr_cb read_cb;
  cntlr_cb after_cb;
  void(*fn)();
} Job;

void rpc_temp_init();
void rpc_handle(String cmdstr);
void rpc_key_handle(String key);
void rpc_draw_handle();
void rpc_handle_for(String cntlr_name, String cmdstr);

#endif
