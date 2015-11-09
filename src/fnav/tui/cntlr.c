#include "fnav/tui/cntlr.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/tui/op_cntlr.h"

#define TABLE_SIZE ARRAY_SIZE(cntlr_table)
struct _cntlr_table {
  String name;
  cntlr_open_cb fn;
} cntlr_table[] = {
  {"fm", fm_init},
  {"sh", sh_init},
  {"op", op_init},
};

void cntlr_load(String name, cntlr_open_cb fn);

Cntlr* cntlr_open(String name, Buffer *buf)
{
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (strcmp(cntlr_table[i].name, name) == 0) {
      return cntlr_table[i].fn(buf);
    }
  }
  return NULL;
}

int cntlr_isloaded(String name)
{
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (strcmp(cntlr_table[i].name, name) == 0) {
      return 1;
    }
  }
  return 0;
}
