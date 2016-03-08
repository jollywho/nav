#ifndef FN_PLUGINS_IMG_H
#define FN_PLUGINS_IMG_H

#include "nav/event/shell.h"

typedef struct Img Img;

struct Img {
  Plugin *base;
  Shell *sh_draw;
  Shell *sh_size;
  Shell *sh_clear;
  Buffer *buf;
  bool disabled;
  bool img_set;
  char *sz_msg;
  char *cl_msg;
  char *img_msg;
  char *path;
  int fontw; int fonth;
  int maxw; int maxh;
  int width; int height;
};

void img_new(Plugin *plugin, Buffer *buf, void *arg);
void img_delete(Plugin *plugin);

#endif
