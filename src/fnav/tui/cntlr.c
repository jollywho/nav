#include "fnav/tui/cntlr.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/tui/op_cntlr.h"
#include "fnav/tui/img_cntlr.h"

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

void cntlr_load(String name, cntlr_open_cb open_cb, cntlr_close_cb close_cb);

static int find_cntlr(String name)
{
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (strcmp(cntlr_table[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

Cntlr* cntlr_open(String name, Buffer *buf)
{
  int i = find_cntlr(name);
  if (i != -1) {
    return cntlr_table[i].open_cb(buf);
  }
  return NULL;
}

void cntlr_close(Cntlr *cntlr)
{
  if (!cntlr) return;
  int i = find_cntlr(cntlr->name);
  if (i != -1) {
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

Cntlr *focus_cntlr()
{
  Buffer *buf = window_get_focus();
  return buf->cntlr;
}

void cntlr_pipe(Cntlr *cntlr)
{
  window_set_status(0, 0, "", "|> op");
}
