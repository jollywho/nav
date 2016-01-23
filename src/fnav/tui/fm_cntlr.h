#ifndef FN_TUI_FM_CNTLR_H
#define FN_TUI_FM_CNTLR_H

#include "fnav/tui/cntlr.h"
#include "fnav/event/fs.h"

typedef struct FM_cntlr FM_cntlr;

struct FM_cntlr {
  Cntlr base;
  int op_count;
  int mo_count;
  String cur_dir;
  fn_fs *fs;
};

void fm_init();
void fm_cleanup();
Cntlr* fm_new(Buffer *buf);
void fm_delete(Cntlr *cntlr);

String fm_cur_dir(Cntlr *cntlr);

#endif
