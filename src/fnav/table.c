#include <stdio.h>
#include "fnav/table.h"
#include "fnav/log.h"

void table_add(Job *job, char **args)
{
  log_msg("TABLE", "add");
  //dowork
  job->read_cb(job->caller, args[0],args[1],args[2]);
}
