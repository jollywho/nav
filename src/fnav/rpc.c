#include <stdio.h>
#include <strings.h>

#include "fnav/rpc.h"
#include "fnav/fm_cntlr.h"
#include "fnav/event.h"
#include "fnav/log.h"

Cntlr *cntlr; //replace later with focus

void rpc_temp_init()
{
  cntlr = (Cntlr*)fm_cntlr_init();
}

// Input handler
void rpc_key_handle(String key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
  cntlr->_input(cntlr, key);
}

void rpc_push_channel(Channel *channel)
{
  //add to list
  log_msg("RPC", "rpc push channel");
  event_push(channel);
}

void rpc_push_job(Job job)
{
  queue_push(&job);
}
