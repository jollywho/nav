#include <malloc.h>
#include <sys/time.h>
#include <libgen.h>
#include <wordexp.h>

#include "fnav/event/fs.h"
#include "fnav/model.h"
#include "fnav/event/event.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/fm_cntlr.h"

#define RFSH_RATE 1000

static void fs_close_req(FS_handle *fsh);
static void stat_cb(uv_fs_t *req);
void fs_loop(Loop *loop, int ms);
static void watch_cb(uv_fs_event_t *hndl, const char *fname, int events, int status);
static void watch_timer_cb(uv_timer_t *handle);

FS_handle* fs_init(fn_handle *h)
{
  log_msg("FS", "open req");
  FS_handle *fsh = malloc(sizeof(FS_handle));
  memset(fsh, 0, sizeof(FS_handle));
  uv_fs_event_init(eventloop(), &fsh->watcher);
  uv_timer_init(eventloop(), &fsh->watcher_timer);
  fsh->hndl = h;
  fsh->running = false;
  fsh->watcher.data = fsh;
  fsh->watcher_timer.data = fsh;
  return fsh;
}

void fs_cleanup(FS_handle *fsh)
{
  log_msg("FS", "cleanup");
  //TODO: spin until loop cleared
  uv_fs_event_stop(&fsh->watcher);
  uv_timer_stop(&fsh->watcher_timer);
  uv_fs_req_cleanup(&fsh->uv_fs);
  free(fsh);
}

char* conspath(const char *str1, const char *str2)
{
  char *result;
  if (strcmp(str1, "/") == 0)
    asprintf(&result, "/%s", str2);
  else
    asprintf(&result, "%s/%s", str1, str2);
  return result;
}

String fs_parent_dir(const String path)
{
  log_msg("FS", "<<PARENT OF>>: %s", path);
  return dirname(path);
}

String fs_expand_path(String path)
{
  wordexp_t p;
  String newpath = NULL;
  if (wordexp(path, &p, 0) == 0) {
    newpath = strdup(p.we_wordv[0]);
  }
  wordfree(&p);
  return newpath;
}

bool isdir(String path)
{
  if (!path) return false;
  ventry *ent = fnd_val("fm_stat", "fullpath", path);
  if (!ent) return false;
  struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
  if (!st) return false;
  if (!st->st_mode) return false;
  return (S_ISDIR(st->st_mode));
}

static void req_stat_cb(uv_fs_t *req)
{
  log_msg("FS", "req_stat_cb");
  FS_handle *fsh = req->data;
  String path = realpath(fsh->path, NULL);
  free(fsh->path);
  if (path) {
    CREATE_EVENT(eventq(), fm_ch_dir, 2, fsh->data, path);
  }
}

void fs_async_open(FS_handle *fsh, Cntlr *cntlr, String path)
{
  log_msg("FS", "fs_async_open");
  uv_fs_req_cleanup(&fsh->uv_fs);
  memset(&fsh->uv_fs, 0, sizeof(uv_fs_t));
  fsh->path = strdup(path);
  fsh->uv_fs.data = fsh;
  fsh->data = cntlr;

  uv_fs_stat(eventloop(), &fsh->uv_fs, path, req_stat_cb);
}

void* fs_vt_stat_resolv(fn_rec *rec, String key)
{
  String str1 = (String)rec_fld(rec, "fullpath");
  ventry *ent = fnd_val("fm_stat", "fullpath", str1);
  struct stat *stat = (struct stat*)rec_fld(ent->rec, "stat");
  return &stat->st_mtim.tv_sec;
}

void fs_signal_handle(void **data)
{
  log_msg("FS", "fs_signal_handle");
  FS_handle *fsh = data[0];
  fn_handle *h = fsh->hndl;
  fn_lis *l = fnd_lis(h->tn, h->key_fld, h->key);
  ventry *head = lis_get_val(l, "dir");
  if (l && head) {
    model_read_entry(h->model, l, head);
  }
  fsh->running = false;
}

void fs_open(FS_handle *fsh, String dir)
{
  log_msg("FS", "fs open %s", dir);
  uv_fs_req_cleanup(&fsh->uv_fs);
  memset(&fsh->uv_fs, 0, sizeof(uv_fs_t));
  fsh->uv_fs.data = fsh;
  fsh->running = true;
  fsh->path = dir;

  uv_fs_stat(eventloop(), &fsh->uv_fs, fsh->path, stat_cb);
  uv_fs_event_start(&fsh->watcher, watch_cb, fsh->path, 1);
}

void fs_close(FS_handle *h)
{
  uv_timer_stop(&h->watcher_timer);
  uv_fs_event_stop(&h->watcher);
}

static int send_stat(FS_handle *fsh, const char *dir, char *upd)
{
  struct stat *s = malloc(sizeof(struct stat));
  if (stat(dir, s) == -1) {
    free(s);
    return 1;
  }

  trans_rec *r = mk_trans_rec(tbl_fld_count("fm_stat"));
  edit_trans(r, "fullpath", (String)dir, NULL);
  edit_trans(r, "update",   NULL,        upd);
  edit_trans(r, "stat",     NULL,        s);
  CREATE_EVENT(eventq(), commit, 2, "fm_stat", r);
  return 0;
}

static void scan_cb(uv_fs_t *req)
{
  log_msg("FS", "--scan--");
  log_msg("FS", "path: %s", req->path);
  uv_dirent_t dent;
  FS_handle *fsh = req->data;

  /* clear outdated records */
  tbl_del_val("fm_files", "dir",      (String)req->path);
  tbl_del_val("fm_stat",  "fullpath", (String)req->path);

  /* add stat. TODO: should reuse uvstat in stat_cb */
  send_stat(fsh, req->path, "yes");

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    int err = 0;
    trans_rec *r = mk_trans_rec(tbl_fld_count("fm_files"));
    edit_trans(r, "name", (String)dent.name, NULL);
    edit_trans(r, "dir",  (String)req->path, NULL);
    String full = conspath(req->path, dent.name);

    ventry *ent = fnd_val("fm_stat", "fullpath", full);
    if (!ent) {
      log_msg("FS", "--fresh stat-- %s", full);
      err = send_stat(fsh, full, "no");
    }

    edit_trans(r, "fullpath", (String)full,   NULL);
    free(full);

    if (err)
      clear_trans(r, 1);
    else {
      CREATE_EVENT(eventq(), commit, 2, "fm_files", r);
    }
  }
  fs_close_req(fsh);
}

static void stat_cb(uv_fs_t *req)
{
  log_msg("FS", "stat cb");
  FS_handle *fsh = req->data;
  uv_stat_t stat = req->statbuf;

  ventry *ent = fnd_val("fm_files", "dir", fsh->path);

  if (ent) {
    ent = fnd_val("fm_stat", "fullpath", fsh->path);
    if (ent) {
      char *upd = (char*)rec_fld(ent->rec, "update");
      if (*upd) {
        struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
        if (stat.st_ctim.tv_sec == st->st_ctim.tv_sec) {
          log_msg("FS", "STAT:NOP");
          return fs_close_req(fsh);
        }
      }
    }
  }

  if (S_ISDIR(stat.st_mode)) {
    uv_fs_req_cleanup(&fsh->uv_fs);
    uv_fs_scandir(eventloop(), &fsh->uv_fs, fsh->path, 0, scan_cb);
  }
  else
    fs_close_req(fsh);
}

static void fs_reopen(FS_handle *fsh)
{
  log_msg("FS", "--reopen--");
  uv_timer_stop(&fsh->watcher_timer);
  uv_fs_event_stop(&fsh->watcher);
  Cntlr *cntlr = buf_cntlr(fsh->hndl->buf);
  void *args[] = {cntlr, strdup(fsh->path)};
  fm_ch_dir(args);
}

static void watch_timer_cb(uv_timer_t *handle)
{
  log_msg("FS", "--watch_timer--");
  FS_handle *fsh = handle->data;
  if (!fsh->running) {
    fs_reopen(fsh);
  }
}

static void watch_cb(uv_fs_event_t *hndl, const char *fname, int events, int status)
{
  log_msg("FS", "--watch--");
  FS_handle *fsh = hndl->data;

  uv_fs_event_stop(&fsh->watcher);
  uv_timer_start(&fsh->watcher_timer, watch_timer_cb, RFSH_RATE, RFSH_RATE);
}

static void fs_close_req(FS_handle *fsh)
{
  log_msg("FS", "reset %s", (char*)fsh->path);
  CREATE_EVENT(eventq(), fs_signal_handle, 1, fsh);
  uv_fs_req_cleanup(&fsh->uv_fs);
}
