#include <stdio.h>
#include "rpc.h"
#include "fm_cntlr.h"

void rpc(string cmdstr)
{
  //TODO: create job_t. dont exec here
  //
  //TODO: match cmdstr to name in cmds_lst
  fm_cmds_lst[0].exec(NULL);
}
