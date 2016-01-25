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

struct fentry {
  String key;

  uv_fs_event_t watcher;
  uv_fs_t uv_fs; //data->req_handle
  uv_timer_t watcher_timer;
  bool cancel;
  bool running;
  int refs;
  fn_fs *listeners;
  UT_hash_handle hh;
};

static fentry *ent_tbl;

#define RFSH_RATE 1000

static void fs_close_req(fentry *ent);
static void stat_cb(uv_fs_t *req);
static void watch_cb(uv_fs_event_t *hndl, const char *fname, int events, int status);

static fentry* fs_mux(fn_fs *fs)
{
  fentry *ent = NULL;
  HASH_FIND_STR(ent_tbl, fs->path, ent);
  if (!ent) {
    ent = malloc(sizeof(fentry));
    ent->running = false;
    ent->cancel = false;
    ent->listeners = NULL;
    ent->key = strdup(fs->path);
    ent->refs = 2;
    uv_fs_event_init(eventloop(), &ent->watcher);
    uv_timer_init(eventloop(), &ent->watcher_timer);
    ent->watcher.data = ent;
    ent->watcher_timer.data = ent;
    ent->uv_fs.data = ent;
    HASH_ADD_STR(ent_tbl, key, ent);
  }

  fn_fs *find = NULL;
  HASH_FIND_PTR(ent->listeners, fs->path, find);
  if (!find) {
    HASH_ADD_STR(ent->listeners, path, fs);
  }
  fs->ent = ent;
  return ent;
}

static void del_ent(uv_handle_t *hndl)
{
  fentry *ent = hndl->data;
  ent->refs--;
  if (ent->refs < 1) {
    free(ent->key);
    free(ent);
  }
}

static void fs_demux(fn_fs *fs)
{
  log_msg("FS", "open req");
  if (!fs->ent) return;
  fentry *ent = fs->ent;

  HASH_DEL(ent->listeners, fs);

  if (HASH_COUNT(ent->listeners) < 1) {
    HASH_DEL(ent_tbl, ent);
    uv_timer_stop(&ent->watcher_timer);
    uv_fs_event_stop(&ent->watcher);
    uv_fs_req_cleanup(&ent->uv_fs);
    uv_handle_t *hw = (uv_handle_t*)&ent->watcher;
    uv_handle_t *ht = (uv_handle_t*)&ent->watcher_timer;
    uv_close(hw, del_ent);
    uv_close(ht, del_ent);
  }

  free(fs->path);
  fs->ent = NULL;
}

fn_fs* fs_init(fn_handle *hndl)
{
  log_msg("FS", "open req");
  fn_fs *fs = malloc(sizeof(fn_fs));
  memset(fs, 0, sizeof(fn_fs));
  fs->hndl = hndl;
  return fs;
}

void fs_cleanup(fn_fs *fs)
{
  log_msg("FS", "cleanup");

  // iterate tbl and delete each ent and its subparts
}

String conspath(const char *str1, const char *str2)
{
  String result;
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

static void stat_cleanup(void **args)
{
  fn_fs *fs = args[0];
  free(args[1]);
  free(fs->readkey);
  uv_fs_req_cleanup(&fs->uv_fs);
}

static void stat_read_cb(uv_fs_t *req)
{
  log_msg("FS", "stat_read_cb");
  fn_fs *fs = req->data;
  uv_stat_t stat = req->statbuf;

  String path = realpath(fs->readkey, NULL);
  log_msg("FS", "req_stat_cb %s", req->path);

  if (path && S_ISDIR(stat.st_mode)) {
    CREATE_EVENT(eventq(), fs->stat_cb, 2, fs->data, path);
    CREATE_EVENT(eventq(), stat_cleanup, 2, fs, path);
  }
  else {
    free(path);
    free(fs->readkey);
    uv_fs_req_cleanup(&fs->uv_fs);
  }
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
  fentry *ent = data[0];

  fn_fs *it = NULL;
  for (it = ent->listeners; it != NULL; it = it->hh.next) {
    fn_handle *h = it->hndl;

    if (it->open_cb) {
      it->open_cb(NULL);
    }
    else {
      model_recv(h->model);
    }
  }
  ent->running = false;
}

bool fs_blocking(fn_fs *fs)
{
  if (!fs->ent) return false;
  return fs->ent->running;
}

void fs_read(fn_fs *fs, String dir)
{
  log_msg("FS", "fs read %s", dir);
  fs->uv_fs.data = fs;
  fs->readkey = strdup(dir);
  uv_fs_stat(eventloop(), &fs->uv_fs, fs->readkey, stat_read_cb);
}

void fs_open(fn_fs *fs, String dir)
{
  log_msg("FS", "fs open %s", dir);
  fs->path = strdup(dir);
  fentry *ent = fs_mux(fs);

  //TODO: if timer is running, reopen immediately
  if (!ent->running) {
    ent->running = true;

    uv_fs_stat(eventloop(), &ent->uv_fs, ent->key, stat_cb);
    uv_fs_event_start(&ent->watcher, watch_cb, ent->key, 1);
  }
}

void fs_close(fn_fs *fs)
{
  fs_demux(fs);
}

static int send_stat(fentry *ent, const char *dir, char *upd)
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
  fentry *ent = req->data;

  /* clear outdated records */
  tbl_del_val("fm_files", "dir",      (String)req->path);
  tbl_del_val("fm_stat",  "fullpath", (String)req->path);

  /* add stat. TODO: should reuse uvstat in stat_cb */
  send_stat(ent, ent->key, "yes");

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    int err = 0;
    trans_rec *r = mk_trans_rec(tbl_fld_count("fm_files"));
    edit_trans(r, "name", (String)dent.name, NULL);
    edit_trans(r, "dir",  (String)req->path, NULL);
    String full = conspath(req->path, dent.name);

    ventry *vent = fnd_val("fm_stat", "fullpath", full);
    if (!vent) {
      log_msg("FS", "--fresh stat-- %s", full);
      err = send_stat(ent, full, "no");
    }

    edit_trans(r, "fullpath", (String)full,   NULL);
    free(full);

    if (err)
      clear_trans(r, 1);
    else {
      CREATE_EVENT(eventq(), commit, 2, "fm_files", r);
    }
  }
  fs_close_req(ent);
}

static void stat_cb(uv_fs_t *req)
{
  log_msg("FS", "stat cb");
  fentry *ent = req->data;
  uv_stat_t stat = req->statbuf;

  ventry *vent = fnd_val("fm_files", "dir", ent->key);

  if (vent) {
    vent = fnd_val("fm_stat", "fullpath", ent->key);
    if (vent) {
      char *upd = (char*)rec_fld(vent->rec, "update");
      if (*upd) {
        struct stat *st = (struct stat*)rec_fld(vent->rec, "stat");
        if (stat.st_ctim.tv_sec == st->st_ctim.tv_sec) {
          log_msg("FS", "STAT:NOP");
          return fs_close_req(ent);
        }
      }
    }
  }

  if (S_ISDIR(stat.st_mode)) {
    uv_fs_req_cleanup(&ent->uv_fs);
    uv_fs_scandir(eventloop(), &ent->uv_fs, ent->key, 0, scan_cb);
  }
  else
    fs_close_req(ent);
}

static void fs_reopen(fentry *ent)
{
  log_msg("FS", "--reopen--");
  uv_timer_stop(&ent->watcher_timer);
  uv_fs_event_stop(&ent->watcher);

  ent->running = true;
  uv_fs_stat(eventloop(), &ent->uv_fs, ent->key, stat_cb);
  uv_fs_event_start(&ent->watcher, watch_cb, ent->key, 1);
}

static void watch_timer_cb(uv_timer_t *handle)
{
  log_msg("FS", "--watch_timer--");
  fentry *ent = handle->data;
  if (!ent->running) {
    fs_reopen(ent);
  }
}

static void watch_cb(uv_fs_event_t *hndl, const char *fname, int events, int status)
{
  log_msg("FS", "--watch--");
  fentry *ent = hndl->data;

  uv_fs_event_stop(&ent->watcher);
  uv_timer_start(&ent->watcher_timer, watch_timer_cb, RFSH_RATE, RFSH_RATE);
}

static void fs_close_req(fentry *ent)
{
  log_msg("FS", "reset %s", ent->key);
  CREATE_EVENT(eventq(), fs_signal_handle, 1, ent);
  uv_fs_req_cleanup(&ent->uv_fs);
}
