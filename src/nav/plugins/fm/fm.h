#ifndef FN_PLUGINS_FM_H
#define FN_PLUGINS_FM_H

#include "nav/plugins/plugin.h"
#include "nav/event/fs.h"

typedef struct jump_item jump_item;
struct jump_item {
  char *path;
  TAILQ_ENTRY(jump_item) ent;
};

typedef struct FM FM;
struct FM {
  Plugin *base;
  char *cur_dir;
  fn_fs *fs;
  int jump_count;
  int jump_max;
  jump_item *jump_cur;
  TAILQ_HEAD(jlst, jump_item) p;
};

void fm_init();
void fm_cleanup();
void fm_new(Plugin *plugin, Buffer *buf, char *arg);
void fm_delete(Plugin *plugin);

#endif
