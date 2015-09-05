#include <stdio.h>
#include "table.h"

void table_add(Job *job)
{
  fprintf(stderr, "table add\n");
  //dowork
  job->read_cb(job->args[0]);
}
