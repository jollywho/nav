#include <stdio.h>
#include "fnav/table.h"
#include "fnav/log.h"

void table_add(Job *job)
{
  log_msg("TABLE", "add");
  //dowork
  job->read_cb(job->args[0]);
}
