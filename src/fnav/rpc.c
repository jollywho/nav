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
void rpc_key_handle(int key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
  cntlr->_input(cntlr, key);
}
