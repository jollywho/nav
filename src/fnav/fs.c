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
  log_msg("FS", "<<PARENT OF>>: %s", path);
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
  if (!rec) return false;
  String full = rec_fld(rec, "fullpath");
  ventry *ent = fnd_val("fm_stat", "fullpath", full);
  struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
  return (S_ISDIR(st->st_mode));
}

void send_stat(FS_req *fq, const char* dir, char* upd)
{
  FS_handle *fh = fq->fs_h;
  struct stat *s = malloc(sizeof(struct stat));
  stat(dir, s);

  trans_rec *r = mk_trans_rec(2);
  edit_trans(r, "fullpath", (void*)dir,NULL);
  edit_trans(r, "update", NULL, (void*)upd);
  edit_trans(r, "stat", NULL,(void*)s);
  CREATE_EVENT(&fh->loop.events, commit, 2, "fm_stat", r);
}

void scan_cb(uv_fs_t* req)
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
    trans_rec *r = mk_trans_rec(3);
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
  fq->close_cb(fq);
}

void stat_cb(uv_fs_t* req)
{
  log_msg("FS", "stat cb");
  FS_req *fq = req->data;
  fq->uv_stat = req->statbuf;

  ventry *ent = fnd_val("fm_files", "dir", fq->req_name);

  if (ent) {
    log_msg("FS", "HAS FM_FILE");
    ent = fnd_val("fm_stat", "fullpath", fq->req_name);
    if (ent) {
      log_msg("FS", "HAS STAT");
      char *upd = (char*)rec_fld(ent->rec, "update");
      log_msg("FS", "HAS UPD of %d", *upd);
      if (*upd) {
        log_msg("FS", "HAS UPD");
        struct stat *st = (struct stat*)rec_fld(ent->rec, "stat");
        if (fq->uv_stat.st_ctim.tv_sec == st->st_ctim.tv_sec) {
          log_msg("FS", "NOP");
          return;
        }
      }
    }
  }

  if (S_ISDIR(fq->uv_stat.st_mode)) {
    uv_fs_scandir(&fq->fs_h->loop.uv, &fq->uv_fs, fq->req_name, 0, scan_cb);
  }
}

void watch_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
  FS_handle *fsh = handle->data;
  log_msg("FS", "--watch--");
  if (events & UV_RENAME)
    log_msg("FS", "=%s= renamed", filename);
  if (events & UV_CHANGE)
    log_msg("FS", "=%s= changed", filename);
  fs_open(fsh, handle->path);
}

void fs_close_cb(FS_req *fq)
{
  log_msg("FS", "reset %s", (char*)fq->req_name);
  free(fq);
}

void fs_open(FS_handle *fsh, String dir)
{
  log_msg("FS", "fs open %s", dir);
  FS_req *fq = malloc(sizeof(FS_req));
  fq->fs_h = fsh;
  fq->req_name = dir;
  fq->uv_fs.data = fq;
  fq->close_cb = fs_close_cb;
  fq->rec = NULL;

  uv_fs_stat(&fq->fs_h->loop.uv, &fq->uv_fs, dir, stat_cb);

  // TODO: check stat on timer and reopen. set on fqclose or stat NOP.
  //uv_fs_event_stop(&fsh->watcher);
  //uv_fs_event_start(&fsh->watcher, watch_cb, dir, 1);
  //uv_unref((uv_handle_t*)&fsh->watcher);
}

void fs_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

FS_handle* fs_init(Cntlr *c, fn_handle *h, cntlr_cb read_cb)
{
  log_msg("FS", "open req");
  FS_handle *fsh = malloc(sizeof(FS_handle));
  loop_add(&fsh->loop, fs_loop);
  uv_fs_event_init(&fsh->loop.uv, &fsh->watcher);
  fsh->watcher.data = fsh;
  return fsh;
}

void fs_cleanup(FS_handle *fsh)
{
  free(fsh);
}
