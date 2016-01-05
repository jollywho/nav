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

static void fm_left();
static void fm_right();

#define KEYS_SIZE ARRAY_SIZE(key_defaults)
static fn_key key_defaults[] = {
  {'h',     fm_left,        0,             BACKWARD},
  {'l',     fm_right,       0,             FORWARD},
};
static fn_keytbl key_tbl;
static short cmd_idx[KEYS_SIZE];
static String active_dir;

void cntlr_cancel(Cntlr *cntlr)
{
  log_msg("FM", "<|_CANCEL_|>");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  self->op_count = 1;
  self->mo_count = 1;
  self->fs->cancel = true;
}

void fm_read_scan()
{
  log_msg("FM", "read");
}

void fm_after_scan()
{
  log_msg("FM", "async done");
}

void cntlr_focus(Cntlr *cntlr)
{
  log_msg("FM", "cntlr_focus");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  active_dir = self->cur_dir;
  buf_refresh(cntlr->hndl->buf);
}

int fm_opendir(Cntlr *cntlr, String path, short arg)
{
  log_msg("FS", "fm_opendir %s", path);
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  fn_handle *h = cntlr->hndl;
  String cur_dir = self->cur_dir;

  if (!self->fs->running) {
    free(cur_dir);
    model_close(h);
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

static void fm_left(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd left");
  fn_handle *h = cntlr->hndl;
  String path = model_curs_value(h->model, "dir");
  fm_opendir(cntlr, path, arg->arg);
}

static void fm_right(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd right");
  fn_handle *h = cntlr->hndl;

  String path = model_curs_value(h->model, "fullpath");
  if (isdir(path))
    fm_opendir(cntlr, path, arg->arg);
  else
    send_hook_msg("fileopen", cntlr, NULL);
}


int cntlr_input(Cntlr *cntlr, int key)
{
  Cmdarg ca;
  int idx = find_command(&key_tbl, key);
  ca.arg = key_defaults[idx].cmd_arg;
  if (idx >= 0) {
    key_defaults[idx].cmd_func(cntlr, &ca);
    return 1;
  }
  // TODO: send to pipe_cntlrs if not consumed
  // if consumed return 1
  return 0;
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
  c->_input = cntlr_input;
  c->_focus = cntlr_focus;
  c->top = fm;
}

Cntlr* fm_init(Buffer *buf)
{
  log_msg("FM_CNTLR", "init");
  key_tbl.tbl = key_defaults;
  key_tbl.cmd_idx = cmd_idx;
  key_tbl.maxsize = KEYS_SIZE;
  input_init_tbl(&key_tbl);
  FM_cntlr *fm = malloc(sizeof(FM_cntlr));
  fm->base.name = "fm";
  fm->base.fmt_name = "   FM    ";
  fm->op_count = 1;
  fm->mo_count = 1;

  if (!active_dir) {
    active_dir = get_current_dir_name();
  }
  fm->cur_dir = strdup(active_dir);

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
  init_fm_hndl(fm, buf, &fm->base, fm->cur_dir);
  model_init(fm->base.hndl);
  model_open(fm->base.hndl);
  buf_set_cntlr(buf, &fm->base);
  hook_init(&fm->base);

  fm->fs = fs_init(fm->base.hndl);
  fs_open(fm->fs, fm->cur_dir);
  return &fm->base;
}

void fm_cleanup(Cntlr *cntlr)
{
  log_msg("FM_CNTLR", "cleanup");
  FM_cntlr *fm = cntlr->top;
  fn_handle *h = cntlr->hndl;
  model_close(h);
  model_cleanup(h);
  //hook remove_caller
  //hook clear
  hook_cleanup(&fm->base);
  fs_cleanup(fm->fs);
  free(h);
  free(fm->cur_dir);
  free(fm);
}
