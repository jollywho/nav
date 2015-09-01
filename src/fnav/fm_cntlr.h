#ifndef FN_CORE_FM_CNTLR_H
#define FN_CORE_FM_CNTLR_H

#include "cntlr.h"

extern void up();
extern void down();

static Cmd fm_cmds_lst[] = {
  { "j",    "down",   0,    down },
  { "k",    "up",     0,    up,  },
};

#endif
