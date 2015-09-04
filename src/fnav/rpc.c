#include <stdio.h>
#include <strings.h>

#include "rpc.h"
#include "fm_cntlr.h"
#include "event.h"

// Input handler
void rpc_key_handle(String key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
  fm_cntlr._input(key);
}

void rpc_push_channel(Channel channel)
{
  //add to list
  fprintf(stderr, "rpc push channel\n");
  event_push(channel);
}

void rpc_push_job(Job job)
{
  queue_push(job);
}
