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
  FS_handle *fs;
};

Cntlr* fm_new(Buffer *buf);
void fm_delete(Cntlr *cntlr);

void fm_req_dir(Cntlr *cntlr, String path);
void fm_ch_dir(void **args);

#endif
