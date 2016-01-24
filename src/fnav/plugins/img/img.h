#ifndef FN_PLUGINS_IMG_H
#define FN_PLUGINS_IMG_H

#include "fnav/event/shell.h"

typedef struct Img Img;

struct Img {
  Plugin *base;
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

void img_new(Plugin *plugin, Buffer *buf);
void img_delete(Plugin *plugin);

#endif
