#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "fnav/table.h"
#include "fnav/log.h"

TBL_rec* table_gen_rec(TBL_handle *tbl_handle)
{
  TBL_rec *src = tbl_handle->tbl_sig;
  TBL_rec *dest = malloc(sizeof(TBL_rec));
  dest->flds_num = src->flds_num;
  dest->fields = malloc(sizeof(TBL_field) * dest->flds_num);
  for(int i = 0; i < dest->flds_num; i++) {
    dest->fields[i] = malloc(sizeof(TBL_field));
    dest->fields[i]->name = strdup(src->fields[i]->name);
    dest->fields[i]->type = src->fields[i]->type;
  }
  return dest;
}

static TBL_rec* add_values(TBL_handle *h, void **args)
{
  TBL_rec *rec = table_gen_rec(h);
  for(int i = 0; i < rec->flds_num; i++) {
    rec->fields[i]->value = args[i];
  }
  return rec;
}

void table_add(Job *job, void **args)
{
  log_msg("TABLE", "add");
  TBL_handle *h = job->caller->tbl_handle;
  TBL_rec *rec = add_values(h, args);

  // TODO: field getter by name
  log_msg("TABLE", "%s", (char*)rec->fields[1]->value);
  job->read_cb(job->caller, rec);
}
