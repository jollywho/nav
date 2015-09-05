#ifndef FN_CORE_FS_H
#define FN_CORE_FS_H

#include "fnav/loop.h"

void fs_open(String dir, cntlr_cb read_cb, cntlr_cb after_cb);
void fs_stat(String dir, Job *job);

#endif
