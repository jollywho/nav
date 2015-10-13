#ifndef FN_CORE_FM_CNTLR_H
#define FN_CORE_FM_CNTLR_H

#include "fnav/cntlr.h"
#include "fnav/fs.h"

typedef struct FM_cntlr FM_cntlr;

struct FM_cntlr {
  Cntlr base;
  Cntlr parent;
  Cntlr child;
  int op_count;
  int mo_count;
  String cur_dir;
  FS_handle *fs;
};

FM_cntlr* fm_cntlr_init(Pane *pane);
void fm_cntlr_cleanup(FM_cntlr *cntlr);

#endif
