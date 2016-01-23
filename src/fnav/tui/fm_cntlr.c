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
#include "fnav/tui/fm_cntlr.h"
#include "fnav/log.h"
#include "fnav/cmd.h"
#include "fnav/option.h"
#include "fnav/compl.h"
#include "fnav/event/shell.h"
#include "fnav/info.h"

static FM_cntlr *active_fm;

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

static void fm_active_dir(FM_cntlr *fm)
{
  if (!active_fm) {
    fm->cur_dir = get_current_dir_name();
  }
  else {
    fm->cur_dir = strdup(active_fm->cur_dir);
  }
  active_fm = fm;
}

void cntlr_cancel(Cntlr *cntlr)
{
  log_msg("FM", "<|_CANCEL_|>");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  self->op_count = 1;
  self->mo_count = 1;
}

void cntlr_focus(Cntlr *cntlr)
{
  log_msg("FM", "cntlr_focus");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  active_fm = self;
  buf_refresh(cntlr->hndl->buf);
}

static int fm_opendir(Cntlr *cntlr, String path, short arg)
{
  log_msg("FM", "fm_opendir %s", path);
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  fn_handle *h = cntlr->hndl;
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

static void fm_left(Cntlr *host, Cntlr *caller)
{
  log_msg("FM", "cmd left");
  fn_handle *h = host->hndl;
  String path = model_curs_value(h->model, "dir");
  fm_opendir(host, path, BACKWARD);
}

static void fm_right(Cntlr *host, Cntlr *caller)
{
  log_msg("FM", "cmd right");
  fn_handle *h = host->hndl;

  String path = model_curs_value(h->model, "fullpath");
  if (isdir(path))
    fm_opendir(host, path, FORWARD);
  else
    send_hook_msg("fileopen", host, NULL);
}

static void fm_ch_dir(void **args)
{
  log_msg("FM_CNTLR", "fm_ch_dir");
  Cntlr *cntlr = args[0];
  String path = args[1];
  fm_opendir(cntlr, path, FORWARD);
}

void fm_req_dir(Cntlr *cntlr, String path)
{
  log_msg("FM_CNTLR", "fm_req_dir");
  if (strcmp(cntlr->name, "fm") != 0) return;

  if (path[0] == '@') {
    path = mark_path(path);
  }

  if (path[0] != '/' && path[0] != '~') {
    path = conspath(fm_cur_dir(cntlr), path);
  }

  String newpath = fs_expand_path(path);
  if (newpath) {
    FM_cntlr *self = (FM_cntlr*)cntlr->top;
    fs_read(self->fs, newpath);
    free(newpath);
  }
}

static String next_valid_path(String path)
{
  String str = strdup(path);
  free(path);

  //TODO: increment '_0' if already exists
  struct stat s;
  while (stat(str, &s) == 0) {
    String tmp;
    asprintf(&tmp, "%s_0", str);
    free(str);
    str = tmp;
  }
  return str;
}

static void fm_paste(Cntlr *host, Cntlr *caller)
{
  log_msg("FM_CNTLR", "fm_paste");
  FM_cntlr *self = (FM_cntlr*)host->top;
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

static void fm_remove(Cntlr *host, Cntlr *caller)
{
  log_msg("FM_CNTLR", "fm_remove");
  String src = model_curs_value(host->hndl->model, "fullpath");

  String cmdstr;
  asprintf(&cmdstr, "!%s %s", p_rm, src);
  log_msg("BUFFER", "%s", cmdstr);
  shell_exec(cmdstr);
  free(cmdstr);
}

static void init_fm_hndl(FM_cntlr *fm, Buffer *b, Cntlr *c, String val)
{
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->tn = "fm_files";
  hndl->buf = b;
  hndl->key_fld = "dir";
  hndl->key = val;
  hndl->fname = "name";
  c->hndl = hndl;
  c->_cancel = cntlr_cancel;
  c->_focus = cntlr_focus;
  c->top = fm;
}

Cntlr* fm_new(Buffer *buf)
{
  log_msg("FM_CNTLR", "init");
  FM_cntlr *fm = malloc(sizeof(FM_cntlr));
  fm->base.name = "fm";
  fm->base.fmt_name = "   FM    ";
  fm->op_count = 1;
  fm->mo_count = 1;

  fm_active_dir(fm);

  init_fm_hndl(fm, buf, &fm->base, fm->cur_dir);
  model_init(fm->base.hndl);
  model_open(fm->base.hndl);
  buf_set_cntlr(buf, &fm->base);
  buf_set_status(buf, 0, fm->cur_dir, 0, 0);
  hook_init(&fm->base);
  hook_add(&fm->base, &fm->base, fm_paste,  "paste");
  hook_add(&fm->base, &fm->base, fm_remove, "remove");
  hook_add(&fm->base, &fm->base, fm_left,   "left");
  hook_add(&fm->base, &fm->base, fm_right,  "right");

  fm->fs = fs_init(fm->base.hndl);
  fm->fs->stat_cb = fm_ch_dir;
  fm->fs->data = &fm->base;
  fs_open(fm->fs, fm->cur_dir);
  return &fm->base;
}

void fm_delete(Cntlr *cntlr)
{
  log_msg("FM_CNTLR", "cleanup");
  FM_cntlr *fm = cntlr->top;
  fn_handle *h = fm->base.hndl;
  model_close(h);
  model_cleanup(h);
  //hook remove_caller
  //hook clear
  hook_cleanup(&fm->base);
  fs_cleanup(fm->fs);
  free(h);
  free(fm);
}

String fm_cur_dir(Cntlr *cntlr)
{
  FM_cntlr *fm = cntlr->top;
  return fm->cur_dir;
}
