#ifndef FN_CORE_FM_CNTLR_H
#define FN_CORE_FM_CNTLR_H

#include "fnav/cntlr.h"
#include "fnav/fs.h"

typedef struct FM_cntlr FM_cntlr;

struct FM_cntlr {
  Cntlr base;
  /* new fields */
  int op_count;
  int mo_count;
  FS_handle *fs;
};

FM_cntlr* fm_cntlr_init();

#endif
