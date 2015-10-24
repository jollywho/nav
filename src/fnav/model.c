// model
// stream and table callbacks should be directed here for
// managing data structures read by attached buffers.
#include <malloc.h>

#include "fnav/model.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/tui/buffer.h"

static void refind_line(fn_lis *lis);
static void generate_lines(Model *m);

struct fn_line {
  int index;
  fn_rec *rec;
  UT_hash_handle hh;
};

struct Model {
  fn_handle *hndl;
  fn_lis *lis;
  fn_rec *cur;
  ventry *head;
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
  fn_line *ln, *tmp;
  HASH_ITER(hh, hndl->model->lines, ln, tmp) {
    HASH_DEL(hndl->model->lines, ln);
    free(ln);
  }
}

void model_read_entry(Model *m, fn_lis *lis, ventry *head)
{
  log_msg("MODEL", "model_read_entry");
  if (!lis->ent) {
    lis->ent = lis_set_val(lis, m->hndl->fname);
  }
  m->head = head;
  m->hndl->model->lis = lis;
  m->cur = lis->rec;
  generate_lines(m);
  refind_line(m->lis);
  buf_full_invalidate(m->hndl->buf, m->lis->index);
}

void model_read_stream(void **arg)
{
}

static void generate_lines(Model *m)
{
  // FIXME: need to roll ent up/down by N
  // also, are index and lnum being mixed?
  // buffer needs ofs, lnum, index
  // ofs   : position of buffer on window
  // lnum  : visible position on buffer
  // index : absolute position in set
  // cur   : current positon/ cursor
  //
  // TODO: generate whole set start to finish
  ventry *it = m->head->next;
  for (int i = 0; i < tbl_ent_count(m->head); ++i) {
    log_msg("BUFFER", "draw %d", i);
    fn_line *ln = malloc(sizeof(fn_line));
    ln->index = i;
    ln->rec = it->rec;
    HASH_ADD_INT(m->lines, index, ln);
    it = it->next;
  }
}

String model_str_line(Model *m, int index)
{
  fn_line *ln;
  HASH_FIND_INT(m->lines, &index, ln);
  return ln ? rec_fld(ln->rec, m->hndl->fname) : NULL;
}

void* model_curs_value(Model *m, String field)
{
  return rec_fld(m->cur, field);
}

void model_set_curs(Model *m, int index)
{
  fn_line *ln;
  HASH_FIND_INT(m->lines, &index, ln);
  m->cur = ln->rec;
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
