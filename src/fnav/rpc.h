#ifndef FN_CORE_RPC_H
#define FN_CORE_RPC_H

#include "fnav.h"

void rpc_handle(String cmdstr);
void rpc_key_handle(String key);
void rpc_handle_for(String cntlr_name, String cmdstr);

#endif
