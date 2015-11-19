#ifndef FN_TUI_IMG_CNTLR_H
#define FN_TUI_IMG_CNTLR_H

#include "fnav/event/pty_process.h"

typedef struct Img_cntlr Img_cntlr;

struct Img_cntlr {
  Cntlr base;
  Loop loop;
  Buffer *buf;
  uv_process_t proc;
  uv_process_options_t opts;
};

Cntlr* img_init(Buffer *buf);
void img_cleanup(Cntlr *cntlr);

#endif
