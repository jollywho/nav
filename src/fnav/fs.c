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

static int FON = 1;
static int FOFF = 0;

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
  String tmp = strdup(path);
  return dirname(tmp);
}

String fs_base_dir(String path)
{
  String tmp = strdup(path);
  return basename(tmp);
}

void send_rec(FS_req *fq, const char* dir, const char* base, int *f)
{
  Cntlr *c = fq->fs_h->job.caller;
  fn_tbl *t = c->hndl->tbl;
  String fullpath = conspath(dir, base);
  struct stat *s = malloc(sizeof(struct stat));
  stat(fullpath, s);

  JobArg *arg = malloc(sizeof(JobArg));
  fn_rec *r = mk_rec(t);
  rec_edit(r, "name", (void*)base);
  rec_edit(r, "path", (void*)fullpath);
  rec_edit(r, "parent", (void*)dir);
  rec_edit(r, "stat", (void*)s);
  rec_edit(r, "scan", (void*)f);
  free(fullpath);
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
  tbl_del_val(t, "parent", (String)req->path);

  /* edit scan flag if record exists, else create it */
  if (fq->rec) {
    log_msg("FS", "--scan path exists--");
    rec_edit(fq->rec, "scan", &FON);
  }
  else {
    log_msg("FS", "--scan path create--");
    String dir = fs_parent_dir(fq->req_name);
    String base = fs_base_dir(fq->req_name);
    send_rec(fq, dir, base, &FON);
    free(dir);
  }

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    send_rec(fq, req->path, dent.name, &FOFF);
  }
  fq->close_cb(fq);
}

void stat_cb(uv_fs_t* req)
{
  log_msg("FS", "stat cb");
  FS_req *fq = req->data;
  fq->uv_stat = req->statbuf;

  Cntlr *c = fq->fs_h->job.caller;
  fn_tbl *t = c->hndl->tbl;
  ventry *ent = fnd_val(t, "path", fq->req_name);

  struct stat *s = malloc(sizeof(struct stat));
  stat(req->path, s);
  if (ent) {
    log_msg("FS", "ENT");
    int *flag = (int*)rec_fld(ent->rec, "scan");
    if (flag) {
      log_msg("FS", "FLAG");
      if (*flag == FON) {
        log_msg("FS", "FLAG ON");
        struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
        if (fq->uv_stat.st_ctim.tv_sec == st->st_ctim.tv_sec) {
          log_msg("FS", "NOP");
          return;
        }
      }
    }
    fq->rec = ent->rec;
  }

  if (S_ISDIR(fq->uv_stat.st_mode))
    uv_fs_scandir(fq->fs_h->loop, &fq->uv_fs, fq->req_name, 0, scan_cb);
}

bool isdir(fn_rec *rec)
{
  struct stat *st = (struct stat*)rec_fld(rec, "stat");
  return (S_ISDIR(st->st_mode));
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
  fq->rec = NULL;

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
