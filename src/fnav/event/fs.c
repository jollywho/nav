#include <malloc.h>
#include <sys/time.h>
#include <libgen.h>

#include <ncurses.h>

#include "fnav/event/fs.h"
#include "fnav/model.h"
#include "fnav/event/event.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/tui/buffer.h"

#define RFRESH_RATE 10000

static void fs_close_req(FS_req *fq);
static void stat_cb(uv_fs_t *req);
void fs_loop(Loop *loop, int ms);
static void watch_cb(uv_fs_event_t *handle, const char *filename, int events,
    int status);
static void watch_timer_cb(uv_timer_t *handle);

FS_handle* fs_init(fn_handle *h)
{
  log_msg("FS", "open req");
  FS_handle *fsh = malloc(sizeof(FS_handle));
  loop_add(&fsh->loop, fs_loop);
  uv_fs_event_init(&fsh->loop.uv, &fsh->watcher);
  uv_timer_init(&fsh->loop.uv, &fsh->watcher_timer);
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
  loop_remove(&fsh->loop);
  free(fsh);
}

char* conspath(const char *str1, const char *str2)
{
  char* result;
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

bool isdir(String path)
{
  ventry *ent = fnd_val("fm_stat", "fullpath", path);
  struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
  return (S_ISDIR(st->st_mode));
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

void fs_open(FS_handle *fsh, const String dir)
{
  log_msg("FS", "fs open %s", dir);
  FS_req *fq = malloc(sizeof(FS_req));
  fq->fs_h = fsh;
  fq->req_name = strdup(dir);
  fq->uv_fs.data = fq;
  fsh->running = true;
  fsh->path = dir;
  fsh->last_ran = os_hrtime();
  fsh->queued = false;

  uv_fs_stat(&fq->fs_h->loop.uv, &fq->uv_fs, dir, stat_cb);
  //uv_fs_event_start(&fsh->watcher, watch_cb, fsh->path, 1);
}

void fs_close(FS_handle *h)
{
  uv_timer_stop(&h->watcher_timer);
  uv_fs_event_stop(&h->watcher);
}

void fs_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

static void send_stat(FS_req *fq, const char *dir, char *upd)
{
  FS_handle *fh = fq->fs_h;
  struct stat *s = malloc(sizeof(struct stat));
  stat(dir, s);

  trans_rec *r = mk_trans_rec(tbl_fld_count("fm_stat"));
  edit_trans(r, "fullpath", (void*)dir,NULL);
  edit_trans(r, "update", NULL, (void*)upd);
  edit_trans(r, "stat", NULL,(void*)s);
  CREATE_EVENT(&fh->loop.events, commit, 2, "fm_stat", r);
}

static void scan_cb(uv_fs_t *req)
{
  log_msg("FS", "--scan--");
  log_msg("FS", "path: %s", req->path);
  uv_dirent_t dent;
  FS_req *fq = req->data;
  FS_handle *fh = fq->fs_h;

  /* clear outdated records */
  tbl_del_val("fm_files", "dir", (String)req->path);
  tbl_del_val("fm_stat", "fullpath", (String)req->path);

  /* add stat. TODO: should reuse uvstat in stat_cb */
  send_stat(fq, req->path, "yes");

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    trans_rec *r = mk_trans_rec(tbl_fld_count("fm_files"));
    edit_trans(r, "name", (void*)dent.name,NULL);
    edit_trans(r, "dir", (void*)req->path,NULL);
    String full = conspath(req->path, dent.name);

    ventry *ent = fnd_val("fm_stat", "fullpath", full);
    if (!ent) {
      log_msg("FS", "--fresh stat-- %s", full);
      send_stat(fq, full, "no");
    }

    edit_trans(r, "fullpath", (void*)full,NULL);
    free(full);
    CREATE_EVENT(&fh->loop.events, commit, 2, "fm_files", r);
  }
  fs_close_req(fq);
}

static void stat_cb(uv_fs_t *req)
{
  log_msg("FS", "stat cb");
  FS_req *fq = req->data;
  fq->uv_stat = req->statbuf;

  ventry *ent = fnd_val("fm_files", "dir", fq->req_name);

  if (ent) {
    ent = fnd_val("fm_stat", "fullpath", fq->req_name);
    if (ent) {
      char *upd = (char*)rec_fld(ent->rec, "update");
      if (*upd) {
        struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
        if (fq->uv_stat.st_ctim.tv_sec == st->st_ctim.tv_sec) {
          log_msg("FS", "STAT:NOP");
          CREATE_EVENT(&fq->fs_h->loop.events, fs_signal_handle, 1, fq->fs_h);
          return;
        }
      }
    }
  }

  if (S_ISDIR(fq->uv_stat.st_mode)) {
    uv_fs_scandir(&fq->fs_h->loop.uv, &fq->uv_fs, fq->req_name, 0, scan_cb);
  }
}

static void fs_reopen(FS_handle *fsh)
{
  uv_timer_stop(&fsh->watcher_timer);
  uv_fs_event_stop(&fsh->watcher);
  model_close(fsh->hndl);
  fs_open(fsh, fsh->path);
}

static void watch_timer_cb(uv_timer_t *handle)
{
  log_msg("FS", "--watch_timer--");
  FS_handle *fsh = handle->data;
  if (!fsh->running)
    fs_reopen(fsh);
}

static void watch_cb(uv_fs_event_t *handle, const char *filename, int events,
    int status)
{
  log_msg("FS", "--watch--");
  FS_handle *fsh = handle->data;

  if (!fsh->queued) {
    int took = (int)(os_hrtime() - fsh->last_ran)/1000000;
    int delay = RFRESH_RATE - took;
    log_msg("FS", "=%d= took", took);
    log_msg("FS", "=%d= delay", delay);
    fsh->queued = true;
    if (took > 0) {
      uv_fs_event_stop(&fsh->watcher);
      //uv_timer_start(&fsh->watcher_timer, watch_timer_cb, delay, delay);
    }
    else
      fs_reopen(fsh);
  }
}

static void fs_close_req(FS_req *fq)
{
  log_msg("FS", "reset %s", (char*)fq->req_name);
  CREATE_EVENT(&fq->fs_h->loop.events, fs_signal_handle, 1, fq->fs_h);
  free(fq->req_name);
  free(fq);
}
