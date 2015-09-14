#define _GNU_SOURCE
#include <malloc.h>
#include <stdio.h>

#include <ncurses.h>

#include "fnav/rpc.h"
#include "fnav/fs.h"
#include "fnav/event.h"
#include "fnav/table.h"
#include "fnav/log.h"

char* concatpath(const char *restrict str1, const char *restrict str2)
{
  size_t l = strlen(str1);
  char *dest = malloc(l + strlen(str2) + 2);
  strcpy(dest, str1);
  strcpy(dest, "/");
  strcpy(dest, str2);
  return dest;
}

void scan_cb(uv_fs_t* req)
{
  log_msg("FS", "--scan--");
  log_msg("FS", "%s", req->path);
  uv_dirent_t dent;
  FS_req *fq = req->data;
  // TODO: delete all records with matching parent

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    JobArg *arg = malloc(sizeof(JobArg));
    fn_rec *r = mk_rec(fq->fs_h->job.caller->tbl);
    char *fp = concatpath(req->path, dent.name);
    rec_edit(r, "parent", (void*)req->path);
    rec_edit(r, "name", (void*)dent.name);
    rec_edit(r, "fullpath", (void*)fp);
    free(fp);
    //args[2] = (void*)&fq->uv_stat;
    arg->rec = r;
    arg->state = REC_INS;
    queue_push(&fq->fs_h->job, arg);
  }
  fq->close_cb(fq);
}

void watch_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
#ifdef NCURSES_ENABLED
  printw("watch proc\n");
#endif
  log_msg("FM", "--watch--");
  if (events & UV_RENAME)
    log_msg("FM", "=%s= renamed", filename);
  if (events & UV_CHANGE)
    log_msg("FM", "=%s= changed", filename);
}

void stat_cb(uv_fs_t* req)
{
  log_msg("FS", "stat cb");
  FS_req *fq= req->data;
  fq->uv_stat = req->statbuf;
  fn_rec **rec = fnd_val(fq->fs_h->job.caller->tbl, "parent", fq->req_name);

  // TODO:  check file privledges
  //        compare mtimes
  if (!rec) {
    if (S_ISDIR(fq->uv_stat.st_mode))
      uv_fs_scandir(fq->fs_h->loop, &fq->uv_fs, fq->req_name, 0, scan_cb);
  }
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

  uv_fs_stat(fq->fs_h->loop, &fq->uv_fs, dir, stat_cb);
  uv_fs_event_stop(&fq->fs_h->watcher);
  uv_fs_event_init(fq->fs_h->loop, &fq->fs_h->watcher);
  uv_fs_event_start(&fq->fs_h->watcher, watch_cb, dir, 0);
}

FS_handle fs_init(Cntlr *c, cntlr_cb read_cb, cntlr_cb after_cb)
{
  log_msg("FS", "open req");
  FS_handle fsh = {
    .loop = eventloop(),
    .job.caller = c,
    .job.fn = commit,
    .job.read_cb = read_cb,
    .job.after_cb = after_cb,
  };
  return fsh;
}
