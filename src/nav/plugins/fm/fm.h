#ifndef FN_PLUGINS_FM_H
#define FN_PLUGINS_FM_H

#include "nav/plugins/plugin.h"
#include "nav/event/fs.h"

typedef struct FM FM;

struct FM {
  Plugin *base;
  char *cur_dir;
  fn_fs *fs;
};

void fm_init();
void fm_cleanup();
void fm_new(Plugin *plugin, Buffer *buf, char *arg);
void fm_delete(Plugin *plugin);

#endif
