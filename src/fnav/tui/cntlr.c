#include "fnav/tui/cntlr.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/tui/op_cntlr.h"
#include "fnav/tui/img_cntlr.h"
#include "fnav/compl.h"

#define TABLE_SIZE ARRAY_SIZE(cntlr_table)
struct _cntlr_table {
  String name;
  cntlr_open_cb open_cb;
  cntlr_close_cb close_cb;
} cntlr_table[] = {
  {"fm", fm_init, fm_cleanup},
  {"sh", sh_init},
  {"op", op_init},
  {"img", img_init, img_cleanup},
};

typedef struct {
  int key;
  Cntlr *cntlr;
  UT_hash_handle hh;
} Cid;

typedef struct _Cid _Cid;
struct _Cid {
  int key;
  LIST_ENTRY(_Cid) ent;
};

int max_id;
LIST_HEAD(ci, _Cid) id_pool;
Cid *id_table;

void cntlr_load(String name, cntlr_open_cb open_cb, cntlr_close_cb close_cb);

static int find_cntlr(String name)
{
  for (int i = 0; i < (int)TABLE_SIZE; i++) {
    if (strcmp(cntlr_table[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static void set_cid(Cntlr *cntlr)
{
  int key;
  Cid *cid = malloc(sizeof(Cid));

  if (!LIST_EMPTY(&id_pool)) {
    _Cid *ret = LIST_FIRST(&id_pool);
    LIST_REMOVE(ret, ent);
    key = ret->key;
    free(ret);
  }
  else
    key = ++max_id;

  cid->key = key;
  cid->cntlr = cntlr;
  cntlr->id = key;
  HASH_ADD_INT(id_table, key, cid);
}

static void unset_cid(Cntlr *cntlr)
{
  Cid *cid;
  int key = cntlr->id;

  HASH_FIND_INT(id_table, &key, cid);
  HASH_DEL(id_table, cid);

  free(cid);
  _Cid *rem = malloc(sizeof(_Cid));

  rem->key = key;
  LIST_INSERT_HEAD(&id_pool, rem, ent);
}

Cntlr* cntlr_open(String name, Buffer *buf)
{
  int i = find_cntlr(name);
  if (i != -1) {
    Cntlr *ret = cntlr_table[i].open_cb(buf);
    set_cid(ret);
    return ret;
  }
  return NULL;
}

void cntlr_close(Cntlr *cntlr)
{
  if (!cntlr) return;
  int i = find_cntlr(cntlr->name);
  if (i != -1) {
    unset_cid(cntlr);
    return cntlr_table[i].close_cb(cntlr);
  }
}

int cntlr_isloaded(String name)
{
  if (find_cntlr(name)) {
    return 1;
  }
  return 0;
}

Cntlr* focus_cntlr()
{
  Buffer *buf = window_get_focus();
  return buf->cntlr;
}

void cntlr_pipe(Cntlr *cntlr)
{
  Buffer *buf = window_get_focus();
  buf_set_status(buf, 0, 0, 0, "|> op");
}

Cntlr* cntlr_from_id(int id)
{
  Cid *cid;
  HASH_FIND_INT(id_table, &id, cid);
  if (cid)
    return cid->cntlr;
  return NULL;
}

void cntlr_list(String line)
{
  compl_new(TABLE_SIZE);
  for (int i = 0; i < (int)TABLE_SIZE; i++) {
    compl_set_index(i, cntlr_table[i].name, 0, NULL);
  }
}


// two types of compl:
//  A) dynamic; rebuild for every keystroke
//  B) static; build at start
//
// search:
//  foreach row in compl
//    int pos = regex_check(cmdline, row->key)
//    if (pos != -1) {
//      add row to compl_matches
//    }
