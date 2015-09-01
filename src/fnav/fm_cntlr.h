#ifndef FN_CORE_FM_CNTLR_H
#define FN_CORE_FM_CNTLR_H

#include "cntlr.h"

extern void up();
extern void down();

static const cmd_t fm_cmds_lst[] = {
  { "up",     0,  up,    0 },
  { "down",   0,  down,  0 },
};

#endif
