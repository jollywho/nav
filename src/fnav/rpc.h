#ifndef FN_CORE_RPC_H
#define FN_CORE_RPC_H

#include <uv.h>
#include "fnav/lib/queue.h"
#include "fnav/fnav.h"
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
  String *args;
  void(*fn)();
} Job;

typedef struct {
  QUEUE node;
  Job *data;
} QueueItem;

typedef void(*channel_cb)();

typedef struct {
  uv_handle_t* handle;
  Job *job;
  channel_cb open_cb;
  channel_cb close_cb;
  String *args;
} Channel;

void rpc_handle(String cmdstr);
void rpc_key_handle(String key);
void rpc_handle_for(String cntlr_name, String cmdstr);

void rpc_push_job(Job job);
void rpc_push_channel(Channel channel);

#endif
