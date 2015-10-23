// model
// stream and table callbacks should be directed here for
// managing data structures read by attached buffers.
#include <malloc.h>

#include "fnav/model.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/tui/buffer.h"

static int refind_line(ventry *ent, int lnum);

struct fn_line {
  int index;
  ventry *data;
  UT_hash_handle hh;
};

struct Model {
  fn_handle *hndl;
  ventry *lis_ent;
  fn_line *lines;
  bool dirty;
};

void model_init(fn_handle *hndl)
{
  Model *model = malloc(sizeof(Model));
  model->hndl = hndl;
  hndl->model = model;
}

void model_open(fn_handle *hndl)
{
  tbl_add_lis(hndl->tn, hndl->key_fld, hndl->key);
}

void model_close(fn_handle *hndl)
{
  // TODO: save old lis attributes in table
  // TODO: free lines
}

void model_read_entry(Model *m, fn_lis *lis)
{
  ventry *ent = NULL;
  int index, lnum;
  lis_data(lis, ent, &index, &lnum);
  if (!ent) {
    ent = lis_set_val(lis, m->hndl->fname);
  }
  lnum = refind_line(ent, lnum);
  buf_full_invalidate(m->hndl->buf, index);
}

void model_read_stream(void **arg)
{
}

fn_line* model_req_line(Model *m, int index)
{
  // create line from entry
  fn_line *ln = malloc(sizeof(fn_line));
  ln->index = index;
  ln->data = m->lis_ent;
  HASH_ADD_INT(m->lines, index, ln);
  return ln;
}

static int refind_line(ventry *ent, int lnum)
{
  log_msg("MODEL", "rewind");
  int count;
  for(count = lnum; count > 0; --count) {
    if (ent->head) break;
    ent = ent->prev;
  }
  return count;
}

static void gen_lines()
{
}
