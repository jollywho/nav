#include <stdio.h>
#include "fnav/table.h"
#include "fnav/log.h"

void table_add(Job *job)
{
  //dowork
  job->read_cb(job->caller, job->args[0], job->args[1]);
}
