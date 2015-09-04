#include <stdio.h>
#include <strings.h>

#include "rpc.h"
#include "fm_cntlr.h"

// this will be replaced by hash search
static Cmd* find_cmd(String *cmdkey, Cntlr *cntlr)
{
  int i, j;
  Cmd *lst = cntlr->cmd_lst;

  for (i = 0; lst[i].name; i++) {
    if (strcasecmp(lst[i].name, cmdkey[0]) != 0) {
      continue;
    }
    for (j = 1; cmdkey[j]; j++) {} /*count*/
    j--;
    if (j >= lst[i].required_parameters) {
      return &lst[i];
    }
  }
  return NULL;
}

// Input handler
void rpc_key_handle(String key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
  fm_cntlr._input(key);
}

// Cmd handler
void rpc_handle(String cmdstr)
{
  Cmd *cmd = find_cmd(&cmdstr, &fm_cntlr);
  if (cmd != NULL){
    cmd->fn(NULL);
  }
  //TODO: check req args.
  //
  //TODO: create job_t. dont exec here.
  //      add args to job
}

// Request a cntlr do a call
// Useful for instructing another cntlr to do something
void rpc_handle_for(String cntlr_name, String cmdstr)
{
  //get cntlr by name
}
