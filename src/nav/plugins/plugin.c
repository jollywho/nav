#include "nav/plugins/plugin.h"
#include "nav/tui/window.h"
#include "nav/plugins/fm/fm.h"
#include "nav/plugins/op/op.h"
#include "nav/plugins/img/img.h"
#include "nav/plugins/term/term.h"
#include "nav/plugins/out/out.h"
#include "nav/plugins/dt/dt.h"
#include "nav/plugins/ed/ed.h"
#include "nav/compl.h"
#include "nav/log.h"
#include "nav/option.h"

typedef struct plugin_ent plugin_ent;
typedef struct {
  int key;
  Buffer *buf;
  UT_hash_handle hh;
} Cid;

typedef struct _Cid _Cid;
struct _Cid {
  int key;
  LIST_ENTRY(_Cid) ent;
};

static struct plugin_ent {
  char *name;
  plugin_init_cb init_cb;
  plugin_open_cb open_cb;
  plugin_close_cb close_cb;
  int type_bg;
} plugin_table[] = {
  {"fm",   fm_init,  fm_new,   fm_delete,   0},
  {"op",   NULL,     op_new,   op_delete,   1},
  {"out",  out_init, out_new,  out_delete,  0},
#if W3M_SUPPORTED
  {"img",  NULL,     img_new,  img_delete,  0},
#endif
  {"ed"  , NULL,     ed_new,   ed_delete,   0},
  {"term", NULL,     term_new, term_delete, 0},
  {"dt",   NULL,     dt_new,   dt_delete,   0},
};

static int max_callable;
static int max_id;
static LIST_HEAD(ci, _Cid) id_pool;
static Cid *id_table;

void plugin_init()
{
  max_callable = LENGTH(plugin_table);
  for (int i = 0; i < LENGTH(plugin_table); i++) {
    if (plugin_table[i].init_cb)
      plugin_table[i].init_cb();
    if (plugin_table[i].type_bg) {
      plugin_open(plugin_table[i].name, 0, 0);
      max_callable--;
    }
  }
}

void plugin_cleanup()
{
  fs_clr_all_cache();
}

static int find_plugin(const char *name)
{
  if (!name)
    return -1;

  for (int i = 0; i < LENGTH(plugin_table); i++) {
    if (strcmp(plugin_table[i].name, name) == 0)
      return i;
  }
  return -1;
}

void obtain_id(Buffer *buf)
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
  cid->buf = buf;
  buf->id = key;
  HASH_ADD_INT(id_table, key, cid);
}

void forefit_id(Buffer *buf)
{
  Cid *cid;
  int key = buf->id;

  HASH_FIND_INT(id_table, &key, cid);
  HASH_DEL(id_table, cid);

  free(cid);
  _Cid *rem = malloc(sizeof(_Cid));

  rem->key = key;
  LIST_INSERT_HEAD(&id_pool, rem, ent);
}

int plugin_requires_buf(const char *name)
{
  int ret = find_plugin(name);
  if (ret == -1)
    return 0;
  return !plugin_table[ret].type_bg;
}

int plugin_open(const char *name, Buffer *buf, char *line)
{
  int i = find_plugin(name);
  if (i == -1)
    return -1;

  log_msg("PLUG", "%s", plugin_table[i].name);
  Plugin *plugin = calloc(1, sizeof(Plugin));
  plugin_table[i].open_cb(plugin, buf, line);
  plugin->name = plugin_table[i].name;
  int id = buf ? buf->id : -1;
  return id;
}

void plugin_close(Plugin *plugin)
{
  if (!plugin)
    return;

  int i = find_plugin(plugin->name);
  if (i == -1)
    return;

  plugin_table[i].close_cb(plugin);
  clear_opts(&plugin->opts);
  free(plugin);
  plugin = NULL;
}

int plugin_isloaded(const char *name)
{
  return find_plugin(name) + 1;
}

nv_option* local_opts()
{
  Buffer *buf = window_get_focus();
  return buf && buf->plugin ? buf->plugin->opts : NULL;
}

Plugin* focus_plugin()
{
  Buffer *buf = window_get_focus();
  if (!buf)
    return NULL;
  return buf->plugin;
}

Buffer* buf_from_id(int id)
{
  Cid *cid;
  HASH_FIND_INT(id_table, &id, cid);
  if (cid)
    return cid->buf;
  return NULL;
}

Plugin* plugin_from_id(int id)
{
  Buffer *buf = buf_from_id(id);
  if (buf)
    return buf->plugin;
  return NULL;
}

int id_from_plugin(Plugin *plug)
{
  return plug->hndl->buf->id;
}

void plugin_list()
{
  int k = 0;
  for (int i = 0; i < LENGTH(plugin_table); i++) {
    if (plugin_table[i].type_bg)
      continue;
    compl_list_add("%s", plugin_table[i].name);
    k++;
  }
}

void win_list()
{
  int i = 0;
  Cid *it;
  for (it = id_table; it != NULL; it = it->hh.next) {
    compl_list_add("%d", it->key);
    char *name = "";
    if (it->buf->plugin)
      name = it->buf->plugin->name;
    compl_set_col(i, "%s", name);
    i++;
  }
}

void type_list()
{
  log_msg("PLUGIN", "type_list");
  Plugin *plug = window_get_plugin();
  if (!plug)
    return;

  int types = tbl_types(plug->hndl->tn);

  if (types & TYP_STR)
    compl_list_add("%s", "name"); //TODO: use field0
  if (types & TYP_STAT) {
    compl_list_add("%s", "size");
    compl_list_add("%s", "type");
    compl_list_add("%s", "ctime");
    compl_list_add("%s", "dir");
  }
}
