#include <stdio.h>
#include <strings.h>
#include "rpc.h"
#include "loop.h"
#include "fm_cntlr.h"

static Cmd* find_cmd(String *cmdkey, Cmd *lst)
{
  int i, j;

  for (i = 0; lst[i].key; i++) {
    if (strcasecmp(lst[i].key, cmdkey[0]) != 0) {
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

void rpc(String cmdstr)
{
  Cmd *cmd = find_cmd(&cmdstr, fm_cmds_lst);
  if (cmd != NULL){
    cmd->exec(NULL);
  }
  //TODO: check req args.
  //
  //TODO: create job_t. dont exec here.
  //      add args to job
}
