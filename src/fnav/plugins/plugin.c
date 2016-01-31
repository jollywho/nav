#include "fnav/plugins/plugin.h"
#include "fnav/tui/window.h"
#include "fnav/plugins/fm/fm.h"
#include "fnav/plugins/op/op.h"
#include "fnav/plugins/img/img.h"
#include "fnav/plugins/term/term.h"
#include "fnav/compl.h"
#include "fnav/log.h"
#include "fnav/option.h"

typedef struct plugin_ent plugin_ent;
typedef struct {
  int key;
  Plugin *plugin;
  UT_hash_handle hh;
} Cid;

typedef struct _Cid _Cid;
struct _Cid {
  int key;
  LIST_ENTRY(_Cid) ent;
};

#define TABLE_SIZE ARRAY_SIZE(plugin_table)
static struct plugin_ent {
  String name;
  plugin_open_cb open_cb;
  plugin_close_cb close_cb;
  int type_bg;
} plugin_table[] = {
  {"fm",   fm_new,   fm_delete, 0},
  {"op",   op_new,   op_delete, 1},
  {"img",  img_new,  img_delete, 0},
  {"term", term_new, term_delete, 0},
};

static int max_id;
static LIST_HEAD(ci, _Cid) id_pool;
static Cid *id_table;

static int find_plugin(String name)
{
  for (int i = 0; i < (int)TABLE_SIZE; i++) {
    if (strcmp(plugin_table[i].name, name) == 0)
      return i;
  }
  return -1;
}

static Plugin* find_loaded_plugin(String name)
{
  Cid *it;
  for (it = id_table; it != NULL; it = it->hh.next) {
    if (strcmp(it->plugin->name, name) == 0)
      return it->plugin;
  }
  return NULL;
}

static void set_cid(Plugin *plugin)
{
  int key;
  Cid *cid = malloc(sizeof(Cid));

  if (!LIST_EMPTY(&id_pool)) {
    _Cid *ret = LIST_FIRST(&id_pool);
    LIST_REMOVE(ret, ent);
    key = ret->key;
    free(ret);
  }
  else
    key = ++max_id;

  cid->key = key;
  cid->plugin = plugin;
  plugin->id = key;
  HASH_ADD_INT(id_table, key, cid);
}

static void unset_cid(Plugin *plugin)
{
  Cid *cid;
  int key = plugin->id;

  HASH_FIND_INT(id_table, &key, cid);
  HASH_DEL(id_table, cid);

  free(cid);
  _Cid *rem = malloc(sizeof(_Cid));

  rem->key = key;
  LIST_INSERT_HEAD(&id_pool, rem, ent);
}

static Plugin* plugin_in_bkgrnd(plugin_ent *ent)
{
  if (!ent->type_bg)
    return NULL;
  return find_loaded_plugin(ent->name);
}

Plugin* plugin_open(String name, Buffer *buf)
{
  int i = find_plugin(name);
  if (i == -1)
    return NULL;

  Plugin *plugin = plugin_in_bkgrnd(&plugin_table[i]);
  if (!plugin) {
    plugin = calloc(1, sizeof(Plugin));
    set_cid(plugin);
    plugin_table[i].open_cb(plugin, buf);
  }
  return plugin;
}

void plugin_close(Plugin *plugin)
{
  if (!plugin)
    return;

  int i = find_plugin(plugin->name);
  if (i == -1)
    return;

  unset_cid(plugin);
  return plugin_table[i].close_cb(plugin);
}

int plugin_isloaded(String name)
{
  return find_plugin(name) + 1;
}

Plugin* focus_plugin()
{
  Buffer *buf = window_get_focus();
  return buf->plugin;
}

void plugin_pipe(Plugin *plugin)
{
  Buffer *buf = window_get_focus();
  buf_set_status(buf, 0, 0, 0, "op");
}

Plugin* plugin_from_id(int id)
{
  Cid *cid;
  HASH_FIND_INT(id_table, &id, cid);
  if (cid)
    return cid->plugin;
  return NULL;
}

void plugin_list(List *args)
{
  compl_new(TABLE_SIZE, COMPL_STATIC);
  for (int i = 0; i < (int)TABLE_SIZE; i++) {
    compl_set_index(i, plugin_table[i].name, 0, NULL);
  }
}
