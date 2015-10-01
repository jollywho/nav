#include <stdio.h>
#include <strings.h>

#include "fnav/rpc.h"
#include "fnav/fm_cntlr.h"
#include "fnav/event.h"
#include "fnav/log.h"

Cntlr *focus; //replace later with focus

void rpc_temp_init()
{
  focus = &fm_cntlr_init()->base;
}

// Input handler
void rpc_key_handle(int key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
  if (key == 'n') {
    focus = focus->next;
    focus->_focus(focus);
  }
  focus->_input(focus, key);
}
