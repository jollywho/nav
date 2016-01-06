#ifndef FN_TUI_IMG_CNTLR_H
#define FN_TUI_IMG_CNTLR_H

#include "fnav/event/shell.h"

typedef struct Img_cntlr Img_cntlr;

struct Img_cntlr {
  Cntlr base;
  Shell *sh_draw;
  Shell *sh_size;
  Shell *sh_clear;
  Buffer *buf;
  bool disabled;
  bool img_set;
  String sz_msg;
  String cl_msg;
  String img_msg;
  int fontw; int fonth;
  int maxw; int maxh;
  String path;
  int width; int height;
};

Cntlr* img_new(Buffer *buf);
void img_delete(Cntlr *cntlr);

#endif
