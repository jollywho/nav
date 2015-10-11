#define _GNU_SOURCE
#include <stdio.h>
#include <strings.h>

#include "fnav/rpc.h"
#include "fnav/fm_cntlr.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/pane.h"

// Input handler
void rpc_key_handle(int key)
{
  //TODO: pass to client first
  //      -ret 1 is consumed
  //      -ret 0 is ignored
  //TODO: set focus cntrl
}

trans_rec* mk_trans_rec(int fld_count)
{
  trans_rec *r = malloc(sizeof(trans_rec));
  r->flds = malloc(sizeof(String)*fld_count);
  r->data = malloc(sizeof(void*)*fld_count);
  r->count = 0;
  r->max = fld_count;
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
