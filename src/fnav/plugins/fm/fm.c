#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "fnav/ascii.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/input.h"
#include "fnav/event/hook.h"
#include "fnav/tui/buffer.h"
#include "fnav/plugins/fm/fm.h"
#include "fnav/log.h"
#include "fnav/cmd.h"
#include "fnav/option.h"
#include "fnav/compl.h"
#include "fnav/event/shell.h"
#include "fnav/info.h"

static FM *active_fm;

void fm_init()
{
  active_fm = NULL;
  if (tbl_mk("fm_files")) {
    tbl_mk_fld("fm_files", "name", typSTRING);
    tbl_mk_fld("fm_files", "dir", typSTRING);
    tbl_mk_fld("fm_files", "fullpath", typSTRING);
    tbl_mk_vt_fld("fm_files", "mtime", fs_vt_stat_resolv);
  }

  if (tbl_mk("fm_stat")) {
    tbl_mk_fld("fm_stat", "fullpath", typSTRING);
    tbl_mk_fld("fm_stat", "update", typVOID);
    tbl_mk_fld("fm_stat", "stat", typVOID);
  }
}

void fm_cleanup()
{
  // remove tables
}

static void fm_active_dir(FM *fm)
{
  if (!active_fm) {
    fm->cur_dir = get_current_dir_name();
  }
  else {
    fm->cur_dir = strdup(active_fm->cur_dir);
  }
  active_fm = fm;
}

void plugin_cancel(Plugin *plugin)
{
  log_msg("FM", "<|_CANCEL_|>");
  FM *self = (FM*)plugin->top;
  self->op_count = 1;
  self->mo_count = 1;
}

void plugin_focus(Plugin *plugin)
{
  log_msg("FM", "plugin_focus");
  FM *self = (FM*)plugin->top;
  active_fm = self;
  buf_refresh(plugin->hndl->buf);
}

static int fm_opendir(Plugin *plugin, String path, short arg)
{
  log_msg("FM", "fm_opendir %s", path);
  FM *self = (FM*)plugin->top;
  fn_handle *h = plugin->hndl;
  String cur_dir = self->cur_dir;

  if (!fs_blocking(self->fs)) {
    model_close(h);
    free(cur_dir);
    cur_dir = strdup(path);
    if (arg == BACKWARD)
      cur_dir = fs_parent_dir(cur_dir);
    h->key = cur_dir;
    model_open(h);
    buf_set_status(h->buf, 0, h->key, 0, 0);
    fs_close(self->fs);
    fs_open(self->fs, cur_dir);
    self->cur_dir = cur_dir;
    return 1;
  }
  return 0;
}

static void fm_left(Plugin *host, Plugin *caller, void *data)
{
  log_msg("FM", "cmd left");
  fn_handle *h = host->hndl;
  String path = model_curs_value(h->model, "dir");
  fm_opendir(host, path, BACKWARD);
}

static void fm_right(Plugin *host, Plugin *caller, void *data)
{
  log_msg("FM", "cmd right");
  fn_handle *h = host->hndl;

  String path = model_curs_value(h->model, "fullpath");
  if (isdir(path))
    fm_opendir(host, path, FORWARD);
  else
    send_hook_msg("fileopen", host, NULL, NULL);
}

static void fm_ch_dir(void **args)
{
  log_msg("FM_plugin", "fm_ch_dir");
  Plugin *plugin = args[0];
  String path = args[1];
  fm_opendir(plugin, path, FORWARD);
}

static void fm_req_dir(Plugin *plugin, Plugin *caller, void *data)
{
  log_msg("FM_plugin", "fm_req_dir");
  String path = strdup(data);

  if (path[0] == '@') {
    SWAP_ALLOC_PTR(path, strdup(mark_path(path)));
  }

  if (path[0] != '/' && path[0] != '~') {
    SWAP_ALLOC_PTR(path, conspath(fm_cur_dir(plugin), path));
  }

  String newpath = fs_expand_path(path);
  if (newpath) {
    FM *self = (FM*)plugin->top;
    fs_read(self->fs, newpath);
    free(newpath);
  }
  free(path);
}

static String next_valid_path(String path)
{
  String str = strdup(path);
  free(path);

  //TODO: increment '_0' if already exists
  struct stat s;
  while (stat(str, &s) == 0) {
    String next;
    asprintf(&next, "%s_0", str);
    SWAP_ALLOC_PTR(str, next);
  }
  return str;
}

static void fm_paste(Plugin *host, Plugin *caller, void *data)
{
  log_msg("FM_plugin", "fm_paste");
  FM *self = (FM*)host->top;
  fn_reg *reg = reg_get(host->hndl, "0");
  if (!reg) return;

  //FIXME: handle deleted records
  String src = rec_fld(reg->rec, "fullpath");
  String name = rec_fld(reg->rec, "name");
  log_msg("BUFFER", "{%s} |%s|", reg->key, src);
  String dest;
  asprintf(&dest, "%s/%s", self->cur_dir, name);
  dest = next_valid_path(dest);

  String cmdstr;
  asprintf(&cmdstr, "!%s %s %s", p_cp, src, dest);

  log_msg("BUFFER", "%s", cmdstr);
  shell_exec(cmdstr);
  free(dest);
  free(cmdstr);
}

static void fm_remove(Plugin *host, Plugin *caller, void *data)
{
  log_msg("FM_plugin", "fm_remove");
  String src = model_curs_value(host->hndl->model, "fullpath");

  String cmdstr;
  asprintf(&cmdstr, "!%s %s", p_rm, src);
  log_msg("BUFFER", "%s", cmdstr);
  shell_exec(cmdstr);
  free(cmdstr);
}

static void init_fm_hndl(FM *fm, Buffer *b, Plugin *c, String val)
{
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->tn = "fm_files";
  hndl->buf = b;
  hndl->key_fld = "dir";
  hndl->key = val;
  hndl->fname = "name";
  c->hndl = hndl;
  c->_cancel = plugin_cancel;
  c->_focus = plugin_focus;
  c->top = fm;
}

void fm_new(Plugin *plugin, Buffer *buf)
{
  log_msg("FM_plugin", "init");
  FM *fm = malloc(sizeof(FM));
  fm->base = plugin;
  plugin->name = "fm";
  plugin->fmt_name = "FM";
  fm->op_count = 1;
  fm->mo_count = 1;

  fm_active_dir(fm);

  init_fm_hndl(fm, buf, plugin, fm->cur_dir);
  model_init(plugin->hndl);
  model_open(plugin->hndl);
  buf_set_plugin(buf, plugin);
  buf_set_status(buf, 0, fm->cur_dir, 0, 0);
  hook_init(plugin);
  hook_add(plugin, plugin, fm_paste,   "paste");
  hook_add(plugin, plugin, fm_remove,  "remove");
  hook_add(plugin, plugin, fm_left,    "left");
  hook_add(plugin, plugin, fm_right,   "right");
  hook_add(plugin, plugin, fm_req_dir, "open");

  fm->fs = fs_init(plugin->hndl);
  fm->fs->stat_cb = fm_ch_dir;
  fm->fs->data = plugin;
  fs_open(fm->fs, fm->cur_dir);
}

void fm_delete(Plugin *plugin)
{
  log_msg("FM_plugin", "cleanup");
  FM *fm = plugin->top;
  fn_handle *h = fm->base->hndl;
  model_close(h);
  model_cleanup(h);
  //hook remove_caller
  //hook clear
  hook_cleanup(fm->base);
  fs_cleanup(fm->fs);
  free(h);
  free(fm);
}

String fm_cur_dir(Plugin *plugin)
{
  FM *fm = plugin->top;
  return fm->cur_dir;
}
