#include <stdio.h>
#include <strings.h>

#include "rpc.h"
#include "fm_cntlr.h"

// Input handler
void rpc_key_handle(String key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
  fm_cntlr._input(key);
}

void rpc_push_channel(Channel chan)
{
}

void rpc_push_job(Job job)
{
}
