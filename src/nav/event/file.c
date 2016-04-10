#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <uv.h>
#include "nav/nav.h"
#include "nav/log.h"
#include "nav/macros.h"
#include "nav/event/event.h"
#include "nav/event/fs.h"

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)

static void write_cb(uv_fs_t *req);
static void open_cb(uv_fs_t *req);

typedef struct File File;
struct File {
  uv_fs_t fs;
  uv_fs_t f1, f2, f3;
  uv_fs_t c1, c2;
  uv_fs_t s1, s2;
  uv_fs_t m1;
  int opencnt;
  uint64_t len;
  uint64_t offset;
  uint64_t blk;
  uv_file u1, u2;
  char *src, *dest;
};

static File file;

static int dest_ver(const char *base, const char *other, size_t baselen)
{
  int version;
  const char *p;

  version = 0;
  if (!strncmp(base, other, baselen) && other[baselen] == '_')
  {
    for (p = &other[baselen+1]; ISDIGIT (*p); ++p) {
      version = version * 10 + *p - '0';
    }
    if (p[0])
      version = 0;
  }
  return version;
}

static void scan_cb(uv_fs_t *req)
{
  if (req->result < 0) {
    log_err("FILE", "scan_cb: |%s|, ", uv_strerror(req->result));
    exit(1);
  }
  char *base = basename(file.src);
  char *dir = dirname(file.src);
  int baselen = strlen(base);
  int max_ver = 0;

  uv_dirent_t dent;
  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    int ver = dest_ver(base, dent.name, baselen);
    if (ver > max_ver)
      max_ver = ver;
  }

  log_msg("FILE", "%s %s ver %d", base, dir, max_ver);
  free(file.dest);
  asprintf(&file.dest, "%s/%s_%d", dir, base, max_ver+1);
  log_msg("FILE", "NEWVER: %s", file.dest);
  uv_fs_req_cleanup(&file.m1);
  uv_fs_open(eventloop(), &file.f3, file.dest, O_CREAT|O_EXCL|O_WRONLY, 0644, open_cb);
}

void find_dest_name(char *path)
{
  uv_fs_scandir(eventloop(), &file.m1, dirname(file.dest), 0, scan_cb);
}

static void file_cleanup()
{
  log_msg("FILE", "close");
  free(file.src);
  free(file.dest);
  uv_fs_req_cleanup(&file.s1);
  uv_fs_req_cleanup(&file.s2);
  uv_fs_req_cleanup(&file.f1);
  uv_fs_req_cleanup(&file.f2);
  uv_fs_req_cleanup(&file.f3);
  uv_fs_req_cleanup(&file.fs);
  uv_fs_req_cleanup(&file.c1);
  uv_fs_req_cleanup(&file.c2);
}

static void close_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "close_cb: |%s|, ", uv_strerror(req->result));
  file.opencnt--;
  if (file.opencnt == 0)
    file_cleanup();
}

static void file_close()
{
  uv_fs_close(eventloop(), &file.c1, file.u1, close_cb);
  uv_fs_close(eventloop(), &file.c2, file.u2, close_cb);
}

static void try_sendfile()
{
  log_msg("FILE", "try %d %d", file.u1, file.u2);
  while(1) {
    int ret = uv_fs_sendfile(eventloop(),
        &file.fs,
        file.u2, file.u1,
        file.offset, file.blk, write_cb);
    if (ret > -1)
      return;
    if (ret == UV_EINTR || ret == UV_EAGAIN)
      continue;
  }
}

static void write_cb(uv_fs_t *req)
{
  if (req->result < 0) {
    log_err("FILE", "write_cb: |%s|, ", uv_strerror(req->result));
    file_close();
  }

  file.offset += req->result;
  //log_msg("FILE", "wrote %ld of %ld", file.offset, file.len);

  if (file.offset < file.len)
    try_sendfile();
  else
    file_close();
}

static void stat_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "stat_cb: |%s|, ", uv_strerror(req->result));

  if (req == &file.s1) {
    file.u1 = req->result;
    file.len = req->statbuf.st_size;
    file.blk = MAX(req->statbuf.st_blksize, 4096);
    log_msg("FILE", "use %ld for %ld", file.blk, file.len);
    uv_fs_open(eventloop(), &file.f1, file.src, O_RDONLY, 0, open_cb);
  }

  if (req == &file.s2) {
    if (S_ISDIR(req->statbuf.st_mode)) {
      SWAP_ALLOC_PTR(file.dest, conspath(file.dest, basename(file.src)));
    }
    log_err("FILE", "open: %s", file.dest);
    uv_fs_open(eventloop(), &file.f2, file.dest, O_CREAT|O_EXCL|O_WRONLY, 0644, open_cb);
  }
}

static void open_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "open_cb: |%s|, ", uv_strerror(req->result));

  if (req == &file.f1)
    file.u1 = req->result;

  if (req == &file.f2 || req == &file.f3) {
    if (req->result == UV_EEXIST) {
      log_err("FILE", "file exists: %s", req->path);
      find_dest_name(file.dest);
      return;
    }
    file.u2 = req->result;
  }

  file.opencnt++;

  if (file.opencnt == 2)
    try_sendfile();
}

void file_copy(const char *src, const char *dest)
{
  log_msg("FILE", "copy |%s|%s|", src, dest);
  file.offset = 0;
  file.opencnt = 0;
  file.u1 = -1;
  file.u2 = -1;
  file.src = strdup(src);
  file.dest = strdup(dest);

  uv_fs_stat(eventloop(), &file.s1, file.src, stat_cb);
  uv_fs_stat(eventloop(), &file.s2, file.dest, stat_cb);
}
