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
  asprintf(&result, "%s/%s", str1, str2);
  return result;
}

void send_rec(uv_fs_t* req, const char* fname)
{
  FS_req *fq = req->data;
  Cntlr *c = fq->fs_h->job.caller;
  fn_tbl *t = c->hndl->tbl;
  String fullpath = conspath(req->path, fname);
  String temp = strdup(req->path);
  temp = dirname(temp);
  struct stat *s = malloc(sizeof(struct stat));
  int *i = malloc(sizeof(int));
  stat(fullpath, s);
  if (S_ISDIR(s->st_mode))
    *i = 1;
  else
    *i = 0;

  JobArg *arg = malloc(sizeof(JobArg));
  fn_rec *r = mk_rec(t);
  rec_edit(r, "name", (void*)fname);
  rec_edit(r, "dir", (void*)req->path);
  rec_edit(r, "fullpath", (void*)fullpath);
  rec_edit(r, "parent", (void*)temp);
  rec_edit(r, "stat", (void*)i);
  free(fullpath);
  free(temp);
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
  Cntlr *c = fq->fs_h->job.caller;
  fn_tbl *t = c->hndl->tbl;
  tbl_del_val(t, "dir", (String)req->path);

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    send_rec(req, dent.name);
  }
  fq->close_cb(fq);
}

void stat_cb(uv_fs_t* req)
{
  log_msg("FS", "stat cb");
  FS_req *fq= req->data;
  fq->uv_stat = req->statbuf;

  if (S_ISDIR(fq->uv_stat.st_mode))
    uv_fs_scandir(fq->fs_h->loop, &fq->uv_fs, fq->req_name, 0, scan_cb);
}

void watch_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
  log_msg("FM", "--watch--");
  if (events & UV_RENAME)
    log_msg("FM", "=%s= renamed", filename);
  if (events & UV_CHANGE)
    log_msg("FM", "=%s= changed", filename);
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
  fq->fs_h->watcher.data = fq;
  fq->close_cb = fs_close_cb;

  Cntlr *c = fq->fs_h->job.caller;
  fn_tbl *t = c->hndl->tbl;
  ventry *ent = fnd_val(t, "dir", fq->req_name);

  if (ent) {
//    uv_stat_t *st = (uv_stat_t*)rec_fld(ent->rec, "stat");
//    struct timeval t;
//    timersub((struct timeval*)&fq->uv_stat.st_mtim, (struct timeval*)&st->st_mtim, &t);
    return;
  }

  uv_fs_stat(fq->fs_h->loop, &fq->uv_fs, dir, stat_cb);
  uv_fs_event_stop(&fq->fs_h->watcher);
  uv_fs_event_init(fq->fs_h->loop, &fq->fs_h->watcher);
  uv_fs_event_start(&fq->fs_h->watcher, watch_cb, dir, 0);
}

FS_handle* fs_init(Cntlr *c, fn_handle *h, cntlr_cb read_cb)
{
  log_msg("FS", "open req");
  FS_handle *fsh = malloc(sizeof(FS_handle));
  fsh->loop = eventloop();
  fsh->job.caller = c;
  fsh->job.hndl = h;
  fsh->job.read_cb = read_cb;
  return fsh;
}
