#ifndef FN_PLUGINS_FM_H
#define FN_PLUGINS_FM_H

#include "fnav/plugins/plugin.h"
#include "fnav/event/fs.h"

typedef struct FM FM;

struct FM {
  Plugin *base;
  int op_count;
  int mo_count;
  String cur_dir;
  fn_fs *fs;
};

void fm_init();
void fm_cleanup();
void fm_new(Plugin *plugin, Buffer *buf);
void fm_delete(Plugin *plugin);

String fm_cur_dir(Plugin *plugin);

#endif
