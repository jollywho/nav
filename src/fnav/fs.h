#ifndef FN_CORE_FS_H
#define FN_CORE_FS_H

#include "fnav/loop.h"

typedef struct FS_handle FS_handle;

struct FS_handle {
  Job *job;
  Channel *channel;
};

FS_handle* fs_init(Cntlr *c, String dir, cntlr_cb read_cb, cntlr_cb after_cb);
void fs_open(FS_handle *h, String dir);

#endif
