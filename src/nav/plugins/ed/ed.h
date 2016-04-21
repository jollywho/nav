#ifndef FN_PLUGINS_ED_H
#define FN_PLUGINS_ED_H

#include <uv.h>
#include "nav/plugins/plugin.h"

typedef struct Ed Ed;

struct Ed {
  Plugin *base;
  Buffer *buf;
  varg_T src;
  varg_T dest;
  char tmp_name[PATH_MAX];
  char curdir[PATH_MAX];
  int bufno;
  int fd;
  int state;
};

void ed_new(Plugin *plugin, Buffer *buf, char *arg);
void ed_delete(Plugin *plugin);
void ed_close_cb(Plugin *plugin, Ed *ed, bool closed);

#endif
