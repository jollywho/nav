#define _GNU_SOURCE
#include <malloc.h>
#include <stdio.h>
#include <sys/time.h>
#include <libgen.h>

#include <ncurses.h>

#include "fnav/rpc.h"
#include "fnav/buffer.h"
#include "fnav/fs.h"
#include "fnav/event.h"
#include "fnav/table.h"
#include "fnav/log.h"

char* conspath(const char* str1, const char* str2)
{
  char* result;
  if (strcmp(str1, "/") == 0) {
    asprintf(&result, "/%s", str2);
  }
  else
    asprintf(&result, "%s/%s", str1, str2);
  return result;
}

String fs_parent_dir(String path)
{
  // TODO: when to free this
  String tmp = strdup(path);
  return dirname(tmp);
}

String fs_base_dir(String path)
{
  String tmp = strdup(path);
  return basename(tmp);
}

bool isdir(fn_rec *rec)
{
  if (!rec) return NULL;
  String full = rec_fld(rec, "fullpath");
  ventry *ent = fnd_val("fm_stat", "fullpath", full);
  struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
  return (S_ISDIR(st->st_mode));
}

void send_stat(FS_req *fq, const char* dir)
{
  struct stat *s = malloc(sizeof(struct stat));
  stat(dir, s);

  JobArg *arg = malloc(sizeof(JobArg));
  arg->tname = "fm_stat";
  fn_rec *r = mk_rec(arg->tname);
  rec_edit(r, "fullpath", (void*)dir);
  rec_edit(r, "stat", (void*)s);
  arg->rec = r;
  arg->fn = commit;
  QUEUE_PUT(work, &fq->fs_h->job, arg);
}

void scan_cb(uv_fs_t* req)
{
  log_msg("FS", "--scan--");
  log_msg("FS", "path: %s", req->path);
  uv_dirent_t dent;
  FS_req *fq = req->data;

  /* clear outdated records */
  tbl_del_val("fm_files", "dir", (String)req->path);

  if (fq->rec) {
    log_msg("FS", "--stat already exists--");
    tbl_del_val("fm_stat", "fullpath", (String)req->path);
  }
  /* add stat. TODO: should reuse uvstat in stat_cb */
  send_stat(fq, req->path);

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    JobArg *arg = malloc(sizeof(JobArg));
    arg->tname = "fm_files";
    fn_rec *r = mk_rec(arg->tname);
    rec_edit(r, "name", (void*)dent.name);
    rec_edit(r, "dir", (void*)req->path);
    String full = conspath(req->path, dent.name);
    tbl_del_val("fm_stat", "fullpath", full);
    send_stat(fq, full);
    rec_edit(r, "fullpath", (void*)full);
    free(full);
    arg->rec = r;
    arg->fn = commit;
    QUEUE_PUT(work, &fq->fs_h->job, arg);
  }
  fq->close_cb(fq);
}

void stat_cb(uv_fs_t* req)
{
  log_msg("FS", "stat cb");
  FS_req *fq = req->data;
  fq->uv_stat = req->statbuf;

  ventry *ent = fnd_val("fm_files", "dir", fq->req_name);

  if (ent) {
    log_msg("FS", "ENT");
    ent = fnd_val("fm_stat", "fullpath", fq->req_name);
    struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
    if (fq->uv_stat.st_ctim.tv_sec == st->st_ctim.tv_sec) {
      log_msg("FS", "NOP");
      return;
    }
    fq->rec = ent->rec;
  }

  if (S_ISDIR(fq->uv_stat.st_mode))
    uv_fs_scandir(fq->fs_h->loop, &fq->uv_fs, fq->req_name, 0, scan_cb);
}

void watch_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
  FS_handle *hndl = handle->data;
  log_msg("FS", "--watch--");
  if (events & UV_RENAME)
    log_msg("FS", "=%s= renamed", filename);
  if (events & UV_CHANGE)
    log_msg("FS", "=%s= changed", filename);
  //fs_open(hndl, handle->path);
}

void fs_close_cb(FS_req *fq)
{
  log_msg("FS", "reset %s", (char*)fq->req_name);
  free(fq);
}

void fs_open(FS_handle *fsh, String dir)
{
  log_msg("FS", "fs open");
  FS_req *fq = malloc(sizeof(FS_req));
  fq->fs_h = fsh;
  fq->req_name = dir;
  fq->uv_fs.data = fq;
  fq->close_cb = fs_close_cb;
  fq->rec = NULL;

  uv_fs_stat(fq->fs_h->loop, &fq->uv_fs, dir, stat_cb);
  uv_fs_event_stop(&fsh->watcher);
  uv_fs_event_init(fsh->loop, &fsh->watcher);
  uv_fs_event_start(&fsh->watcher, watch_cb, dir, 1);
}

FS_handle* fs_init(Cntlr *c, fn_handle *h, cntlr_cb read_cb)
{
  log_msg("FS", "open req");
  FS_handle *fsh = malloc(sizeof(FS_handle));
  fsh->loop = eventloop();
  fsh->job.caller = c;
  fsh->job.hndl = h;
  fsh->job.read_cb = read_cb;
  fsh->watcher.data = fsh;
  return fsh;
}
