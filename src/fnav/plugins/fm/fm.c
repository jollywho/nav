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
#include "fnav/tui/window.h"

void fm_init()
{
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

void plugin_cancel(Plugin *plugin)
{
  log_msg("FM", "<|_CANCEL_|>");
}

void plugin_focus(Plugin *plugin)
{
  log_msg("FM", "plugin_focus");
  FM *self = (FM*)plugin->top;
  window_ch_dir(self->cur_dir);
  buf_refresh(plugin->hndl->buf);
}

static int fm_opendir(Plugin *plugin, char *path, short arg)
{
  log_msg("FM", "fm_opendir %s", path);
  FM *self = (FM*)plugin->top;
  fn_handle *h = plugin->hndl;
  char *cur_dir = self->cur_dir;

  free(cur_dir);
  cur_dir = strdup(path);

  if (arg == BACKWARD)
    cur_dir = fs_parent_dir(cur_dir);

  h->key = cur_dir;
  model_open(h);
  buf_set_status(h->buf, 0, h->key, 0);
  fs_close(self->fs);
  fs_open(self->fs, cur_dir);
  self->cur_dir = cur_dir;
  window_ch_dir(self->cur_dir);
  send_hook_msg("diropen", plugin, NULL, &(HookArg){NULL,self->cur_dir});
  return 1;
}

static void fm_left(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "cmd left");
  FM *self = host->top;
  char *path = strdup(self->cur_dir);
  fm_opendir(host, path, BACKWARD);
  free(path);
}

static void fm_right(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "cmd right");
  fn_handle *h = host->hndl;

  char *path = model_curs_value(h->model, "fullpath");
  if (!path)
    return;

  if (isdir(path))
    fm_opendir(host, path, FORWARD);
  else
    send_hook_msg("fileopen", host, NULL, NULL);
}

static void fm_ch_dir(void **args)
{
  log_msg("FM", "fm_ch_dir");
  Plugin *plugin = args[0];
  char *path = args[1];
  fm_opendir(plugin, path, FORWARD);
}

static char* valid_full_path(char *base, char *path)
{
  if (!path)
    return strdup(base);

  char *dir = fs_expand_path(path);
  if (path[0] == '@') {
    char *tmp = mark_path(dir);
    if (tmp)
      SWAP_ALLOC_PTR(dir, strdup(tmp));
  }
  if (dir[0] != '/')
    SWAP_ALLOC_PTR(dir, conspath(window_cur_dir(), dir));

  char *valid = realpath(dir, NULL);
  if (!valid) {
    free(dir);
    return strdup(base);
  }
  SWAP_ALLOC_PTR(dir, valid);
  return dir;
}

static void fm_req_dir(Plugin *plugin, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_req_dir");
  FM *self = plugin->top;
  if (!hka->arg)
    hka->arg = "~";

  do_events_until(fs_blocking, self->fs);

  char *path = valid_full_path(window_cur_dir(), hka->arg);

  if (path)
    fs_read(self->fs, path);

  free(path);
}

static char* next_valid_path(char *path)
{
  char *str = strdup(path);
  free(path);

  //TODO: increment '_0' if already exists
  struct stat s;
  while (stat(str, &s) == 0) {
    char *next;
    asprintf(&next, "%s_0", str);
    SWAP_ALLOC_PTR(str, next);
  }
  return str;
}

static void fm_paste(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_paste");
  FM *self = (FM*)host->top;

  char *oper = p_cp;
  char *arg = "-r";
  fn_reg *reg = reg_dcur();
  if (!reg)
    return;
  if (reg->key == '1') {
    oper = p_mv;
    arg = "";
  }

  char *name = basename(reg->value);
  log_msg("FM", "{%d} |%s|", reg->key, reg->value);

  char *dest;
  asprintf(&dest, "%s/%s", self->cur_dir, name);
  dest = next_valid_path(dest);

  char *cmdstr;
  asprintf(&cmdstr, "%s %s \"%s\" \"%s\"", oper, arg, reg->value, dest);

  shell_exec(cmdstr, NULL, self->cur_dir, NULL);
  free(dest);
  free(cmdstr);
  fs_fastreq(self->fs);
  reg_clear_dcur();
}

static void fm_remove(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_remove");
  FM *self = (FM*)host->top;
  if (fs_blocking(self->fs))
    return;

  char *src = model_curs_value(host->hndl->model, "fullpath");
  log_msg("FM", "\"%s\"", src);
  if (!src)
    return;
  remove(src);
  fs_fastreq(self->fs);
}

static void fm_diropen_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_diropen_cb");
  FM *self = (FM*)caller->top;
  fs_close(self->fs);
  char *path = strdup(hka->arg);
  fm_opendir(caller, path, BACKWARD);
  free(path);
}

static void fm_cursor_change_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_cursor_change_cb");
  FM *self = (FM*)caller->top;
  fn_handle *h = host->hndl;

  char *path = model_curs_value(h->model, "fullpath");
  if (!path)
    return;

  if (isdir(path)) {
    fs_close(self->fs);
    fm_opendir(caller, path, FORWARD);
  }
}

static void fm_pipe_left(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_pipe_left");
  //TODO: check if host is fm
  hook_add_intl(caller, host, fm_cursor_change_cb, "cursor_change");
}

static void fm_pipe_right(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_pipe_right");
  hook_add_intl(caller, host, fm_diropen_cb, "diropen");
}

static void init_fm_hndl(FM *fm, Buffer *b, Plugin *c, char *val)
{
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->tn = "fm_files";
  hndl->buf = b;
  hndl->key_fld = "dir";
  hndl->key = val;
  hndl->fname = "name";
  hndl->kname = "fullpath";
  c->hndl = hndl;
  c->_cancel = plugin_cancel;
  c->_focus = plugin_focus;
  c->top = fm;
}

void fm_new(Plugin *plugin, Buffer *buf, void *arg)
{
  log_msg("FM", "init");
  FM *fm = malloc(sizeof(FM));
  fm->base = plugin;
  plugin->name = "fm";
  plugin->fmt_name = "FM";

  fm->cur_dir = valid_full_path(window_cur_dir(), arg);

  init_fm_hndl(fm, buf, plugin, fm->cur_dir);
  model_init(plugin->hndl);
  model_open(plugin->hndl);
  buf_set_plugin(buf, plugin);
  buf_set_status(buf, 0, fm->cur_dir, 0);
  hook_init_host(plugin);
  hook_add_intl(plugin, plugin, fm_paste,      "paste"     );
  hook_add_intl(plugin, plugin, fm_remove,     "remove"    );
  hook_add_intl(plugin, plugin, fm_left,       "left"      );
  hook_add_intl(plugin, plugin, fm_right,      "right"     );
  hook_add_intl(plugin, plugin, fm_req_dir,    "open"      );
  hook_add_intl(plugin, plugin, fm_pipe_left,  "pipe_left" );
  hook_add_intl(plugin, plugin, fm_pipe_right, "pipe_right");

  fm->fs = fs_init(plugin->hndl);
  fm->fs->stat_cb = fm_ch_dir;
  fm->fs->data = plugin;
  fs_open(fm->fs, fm->cur_dir);
  window_ch_dir(fm->cur_dir);
}

void fm_delete(Plugin *plugin)
{
  log_msg("FM", "delete");
  FM *fm = plugin->top;
  fn_handle *h = fm->base->hndl;
  do_events_until(fs_blocking, fm->fs);
  model_close(h);
  model_cleanup(h);
  hook_clear_host(fm->base);
  hook_cleanup_host(fm->base);
  fs_cleanup(fm->fs);
  free(fm->cur_dir);
  free(h);
  free(fm);
}
