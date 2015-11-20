#ifndef FN_TUI_IMG_CNTLR_H
#define FN_TUI_IMG_CNTLR_H

#include "fnav/event/shell.h"

typedef struct Img_cntlr Img_cntlr;

struct Img_cntlr {
  Cntlr base;
  Shell *sh;
  Buffer *buf;
  bool disabled;
};

Cntlr* img_init(Buffer *buf);
void img_cleanup(Cntlr *cntlr);

#endif
