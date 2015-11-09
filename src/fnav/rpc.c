#include <stdio.h>
#include <strings.h>
#include <ctype.h>

#include "fnav/rpc.h"
#include "fnav/log.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/event/event.h"

trans_rec* mk_trans_rec(int fld_count)
{
  trans_rec *r = malloc(sizeof(trans_rec));
  r->max = fld_count;
  r->flds = malloc(sizeof(String)*r->max);
  r->data = malloc(sizeof(void**)*r->max);
  r->count = 0;
  return r;
}

void edit_trans(trans_rec *r, String fname, String val, void *data)
{
  r->flds[r->count] = fname;
  if (!data) {
    r->data[r->count] = strdup(val);
  }
  else {
    r->data[r->count] = data;
  }
  r->count++;
}

void clear_trans(trans_rec *r)
{
  free(r->flds);
  free(r->data);
  free(r);
}
