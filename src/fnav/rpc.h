#ifndef FN_CORE_RPC_H
#define FN_CORE_RPC_H

#include "fnav.h"
#include "loop.h"

//  +----<-----+---->-----+----<-----+
//  |  Event   |          |   Spin   |
//  v          ^   RPC    v          ^
//  |  Loop    |          |   Loop   |
//  +---->-----+----<-----+---->-----+
//
//  +----------+----------+----------+
//  | Channels |  Input   |   Jobs   |
//  +----------+----------+----------+

void rpc_handle(String cmdstr);
void rpc_key_handle(String key);
void rpc_handle_for(String cntlr_name, String cmdstr);

void rpc_start_job(Job job);
void rpc_start_channel(Channel channel);

#endif
