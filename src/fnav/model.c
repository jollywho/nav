// model
// stream and table callbacks should be directed here for
// managing data structures read by attached buffers.
#include <malloc.h>

#include "fnav/model.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/tui/buffer.h"

static void refind_line(fn_lis *lis);

struct fn_line {
  int index;
  ventry *data;
  UT_hash_handle hh;
};

struct Model {
  fn_handle *hndl;
  fn_lis *lis;
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
  log_msg("MODEL", "model_read_entry");
  if (!lis->ent) {
    lis->ent = lis_set_val(lis, m->hndl->fname);
  }
  m->lis = lis;
  refind_line(m->lis);
  buf_full_invalidate(m->hndl->buf, m->lis->index);
}

void model_read_stream(void **arg)
{
}

void model_req_line(Model *m, int index)
{
  log_msg("MODEL", "model_req_line");
  // create line from entry
  fn_line *ln = malloc(sizeof(fn_line));
  ln->index = index;
  ln->data = m->lis->ent;
  HASH_ADD_INT(m->lines, index, ln);
}

String model_str_line(Model *m, int index)
{
  fn_line *ln;
  HASH_FIND_INT(m->lines, &index, ln);
  return ent_str(ln->data);
}

static void refind_line(fn_lis *lis)
{
  log_msg("MODEL", "rewind");
  int count;
  for(count = lis->lnum; count > 0; --count) {
    if (lis->ent->head) break;
    lis->ent = lis->ent->prev;
  }
  lis->lnum = count;
}
