#include "nav/ascii.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/event/input.h"
#include "nav/event/hook.h"
#include "nav/tui/buffer.h"
#include "nav/plugins/fm/fm.h"
#include "nav/log.h"
#include "nav/cmd.h"
#include "nav/option.h"
#include "nav/compl.h"
#include "nav/event/shell.h"
#include "nav/info.h"
#include "nav/tui/message.h"
#include "nav/event/file.h"
#include "nav/util.h"

void fm_init()
{
  if (tbl_mk("fm_files")) {
    tbl_mk_fld("fm_files", "name",     TYP_STR);
    tbl_mk_fld("fm_files", "dir",      TYP_STR);
    tbl_mk_fld("fm_files", "fullpath", TYP_STR);
    tbl_mk_fld("fm_files", "stat",     TYP_STAT);
  }
}

void fm_cleanup()
{
}

void plugin_cancel(Plugin *plugin)
{
  log_msg("FM", "<|_CANCEL_|>");
  FM *self = (FM*)plugin->top;
  Buffer *buf = plugin->hndl->buf;
  file_cancel(buf);
  fs_cancel(self->fs);
}

void plugin_focus(Plugin *plugin)
{
  log_msg("FM", "plugin_focus");
  FM *self = (FM*)plugin->top;
  fs_cwd(self->cur_dir);
  model_ch_focus(plugin->hndl);
  buf_refresh(plugin->hndl->buf);
}

static void jump_init(FM *self)
{
  TAILQ_INIT(&self->p);
  self->jump_max = get_opt_uint("jumplist");
  self->jump_count = 0;
  self->jump_cur = NULL;
}

static void jump_cleanup(FM *self)
{
  while (!TAILQ_EMPTY(&self->p)) {
    jump_item *it = TAILQ_FIRST(&self->p);
    TAILQ_REMOVE(&self->p, it, ent);
    free(it->path);
    free(it);
  }
}

static void jump_push(FM *self, char *path)
{
  log_msg("FM", "jump_push");

  if (self->jump_cur && !strcmp(self->jump_cur->path, path))
    return;

  jump_item *item = malloc(sizeof(jump_item));
  item->path = strdup(path);
  self->jump_count++;
  self->jump_cur = item;
  TAILQ_INSERT_TAIL(&self->p, item, ent);

  if (self->jump_count < self->jump_max)
    return;

  self->jump_max = get_opt_uint("jumplist");
  if (self->jump_count > self->jump_max) {
    log_msg("FM", "jumplist maxexceed");
    jump_item *first = TAILQ_FIRST(&self->p);
    TAILQ_REMOVE(&self->p, first, ent);
    free(first->path);
    free(first);
    self->jump_count--;
  }
}

static int fm_opendir(Plugin *plugin, char *path, short arg)
{
  log_msg("FM", "fm_opendir %s", path);
  FM *self = (FM*)plugin->top;
  Handle *h = plugin->hndl;
  char *cur_dir = self->cur_dir;

  model_close(h);
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
  fs_cwd(self->cur_dir);
  send_hook_msg(EVENT_DIROPEN, plugin, NULL, &(HookArg){NULL,self->cur_dir});
  return 1;
}

static void fm_ch_dir(void **args)
{
  log_msg("FM", "fm_ch_dir");
  Plugin *plugin = args[0];
  FM *self = plugin->top;
  char *path = args[1];
  uv_stat_t *stat = args[2];

  if (!stat) {
    nv_err("not a valid path: %s", path);
    return;
  }

  if (S_ISDIR(stat->st_mode)) {
    jump_push(self, path);
    fm_opendir(plugin, path, FORWARD);
  }
  else
    send_hook_msg(EVENT_FILEOPEN, plugin, NULL, &(HookArg){NULL,path});
}

static void fm_req_dir(Plugin *plugin, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_req_dir");
  FM *self = plugin->top;
  char *req = hka->arg;

  if (!req)
    req = "~";

  DO_EVENTS_UNTIL(!fs_blocking(self->fs));

  char *trypath = add_quotes(req);
  char *path = fs_trypath(trypath);
  if (!path) {
    free(path);
    path = fs_trypath(req);
  }

  //NOTE: files could potentially be opened at this stage from their extension.
  //      the extra FS operations cause extra delay, especially noticeable on
  //      slow networks. could possibly resolve entering an unresponsive state.
  if (path)
    fs_read(self->fs, path);
  else
    nv_err("not a valid path: %s", req);

  free(trypath);
  free(path);
}

static void fm_left(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "left");
  FM *self = host->top;
  char *path = self->cur_dir;
  path = fs_parent_dir(strdup(path));
  fm_req_dir(host, NULL, &(HookArg){NULL,path});
  free(path);
}

static void fm_right(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "right");
  Handle *h = host->hndl;
  char *path = model_curs_value(h->model, "fullpath");
  log_msg("FM", "PATH %s", path);
  if (!path)
    return;
  fm_req_dir(host, NULL, &(HookArg){NULL,path});
}

static void fm_jump(Plugin *plugin, Plugin *caller, HookArg *hka)
{
  log_msg("WINDOW", "fm_jump");
  FM *self = plugin->top;
  jump_item *item = NULL;

  if (hka->ka->arg == BACKWARD)
    item = TAILQ_PREV(self->jump_cur, jlst, ent);
  if (hka->ka->arg == FORWARD)
    item = TAILQ_NEXT(self->jump_cur, ent);

  if (!item)
    item = self->jump_cur;
  else
    self->jump_cur = item;

  hka->arg = item->path;
  fm_req_dir(plugin, caller, hka);
}

static void fm_paste(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_paste");
  FM *self = (FM*)host->top;

  nv_reg *reg = reg_dcur();
  if (!reg || reg->value.argc < 1)
    return;

  Buffer *buf = host->hndl->buf;

  if (reg->key != '1')
    file_copy(reg->value, self->cur_dir, buf);
  else {
    file_move(reg->value, self->cur_dir, buf);
  }
  reg_clear_dcur();

  return;
}

static void fm_remove(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("FM", "fm_remove");
  FM *self = (FM*)host->top;
  if (fs_blocking(self->fs))
    return;

  int count = buf_sel_count(host->hndl->buf);
  varg_T args = buf_select(host->hndl->buf, "fullpath", NULL);
  char *src = lines2argv(args.argc, args.argv);
  log_msg("FM", "src |%s|", src);

  if (get_opt_int("ask_delete")) {
    bool ans = 0;

    if (count > 1)
      ans = confirm("Remove: %d items ?", count);
    else
      ans = confirm("Remove: %s ?", src);

    if (!ans)
      goto cleanup;
  }
  buf_end_sel(host->hndl->buf);

  char *cmdstr;
  asprintf(&cmdstr, "%s %s", p_rm, src);
  fs_fastreq(self->fs);
  shell_exec(cmdstr, fs_pwd());
  free(cmdstr);
cleanup:
  del_param_list(args.argv, args.argc);
  free(src);
}

static void init_fm_hndl(FM *fm, Buffer *b, Plugin *c, char *val)
{
  Handle *hndl = malloc(sizeof(Handle));
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

void fm_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("FM", "init");
  FM *fm = malloc(sizeof(FM));
  fm->base = plugin;
  plugin->name = "fm";
  plugin->fmt_name = "FM";

  /* check manually because won't be initialized to use req_dir */
  char *path = fs_trypath(arg);
  if (path)
    fm->cur_dir = path;
  else {
    nv_err("not a valid path: %s", arg);
    fm->cur_dir = strdup(fs_pwd());
  }

  jump_init(fm);
  init_fm_hndl(fm, buf, plugin, fm->cur_dir);
  model_init(plugin->hndl);
  model_inherit(plugin->hndl);
  model_open(plugin->hndl);
  buf_set_plugin(buf, plugin, SCR_FILE);
  buf_set_status(buf, 0, fm->cur_dir, 0);
  hook_add_intl(buf->id, plugin, plugin, fm_paste,   EVENT_PASTE );
  hook_add_intl(buf->id, plugin, plugin, fm_remove,  EVENT_REMOVE);
  hook_add_intl(buf->id, plugin, plugin, fm_left,    EVENT_LEFT  );
  hook_add_intl(buf->id, plugin, plugin, fm_right,   EVENT_RIGHT );
  hook_add_intl(buf->id, plugin, plugin, fm_req_dir, EVENT_OPEN  );
  hook_add_intl(buf->id, plugin, plugin, fm_jump,    EVENT_JUMP  );

  fm->fs = fs_init(plugin->hndl);
  fm->fs->stat_cb = fm_ch_dir;
  fm->fs->data = plugin;
  fm_req_dir(plugin, NULL, &(HookArg){NULL,fm->cur_dir});
  fs_cwd(fm->cur_dir);
}

void fm_delete(Plugin *plugin)
{
  log_msg("FM", "delete");
  FM *fm = plugin->top;
  Handle *h = plugin->hndl;
  DO_EVENTS_UNTIL(!fs_blocking(fm->fs));
  jump_cleanup(fm);
  model_close(h);
  model_cleanup(h);
  fs_cleanup(fm->fs);
  free(fm->cur_dir);
  free(h);
  free(fm);
}
