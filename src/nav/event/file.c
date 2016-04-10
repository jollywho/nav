#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <uv.h>
#include "nav/nav.h"
#include "nav/log.h"
#include "nav/event/event.h"

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)

static void write_cb(uv_fs_t *req);

typedef struct File File;
struct File {
  uv_fs_t fs;
  uv_fs_t f1, f2;
  uv_fs_t c1, c2;
  uv_fs_t s1;
  int opencnt;
  uint64_t len;
  uint64_t offset;
  uint64_t blk;
  uv_file u1, u2;
  char *src, *dest;
};

static File file;


static int dest_ver(const char *base, const char *backup, size_t base_length)
{
  int version;
  const char *p;

  version = 0;
  if (strncmp(base, backup, base_length) == 0
      && backup[base_length] == '.'
      && backup[base_length + 1] == '~')
  {
    for (p = &backup[base_length + 2]; ISDIGIT (*p); ++p)
      version = version * 10 + *p - '0';
    if (p[0] != '~' || p[1])
      version = 0;
  }
  return version;
}

char* max_dest_ver(char *path)
{
  return 0;
}

char* dest_name(char *path)
{
  return 0;
}

static void close_file()
{
  log_msg("FILE", "close");
  free(file.src);
  free(file.dest);
  uv_fs_req_cleanup(&file.f1);
  uv_fs_req_cleanup(&file.f2);
  uv_fs_req_cleanup(&file.s1);
  uv_fs_req_cleanup(&file.fs);
  uv_fs_req_cleanup(&file.c1);
  uv_fs_req_cleanup(&file.c2);
}

static void close_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_msg("?", "file: |%s|, ", uv_strerror(req->result));
  file.opencnt--;
  if (file.opencnt == 0)
    close_file();
}

static void try_sendfile()
{
  log_msg("FILE", "try");
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
  if (req->result < 0)
    log_msg("?", "file: |%s|, ", uv_strerror(req->result));
  log_msg("FILE", "wrote %ld %ld", file.offset, file.len);

  file.offset += req->result;

  if (file.offset < file.len)
    try_sendfile();
  else {
    uv_fs_close(eventloop(), &file.c1, file.u1, close_cb);
    uv_fs_close(eventloop(), &file.c2, file.u2, close_cb);
  }
}

static void stat_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_msg("?", "stat: |%s|, ", uv_strerror(req->result));
  file.len = req->statbuf.st_size;
  file.blk = MAX(req->statbuf.st_blksize, 4096);
  log_msg("FILE", "use %ld for %ld", file.blk, file.len);

  try_sendfile();
}

static void open_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_msg("?", "open: |%s|, ", uv_strerror(req->result));

  if (req == &file.f1)
    file.u1 = req->result;
  if (req == &file.f2)
    file.u2 = req->result;

  file.opencnt++;

  if (file.opencnt == 2)
    uv_fs_stat(eventloop(), &file.s1, file.src, stat_cb);
}

void file_copy(char *src, char *dest)
{
  log_msg("FILE", "copy");
  file.offset = 0;
  file.src = strdup(src);
  file.dest = strdup(dest);
  uv_fs_open(eventloop(), &file.f1, src, O_RDONLY, 0, open_cb);
  uv_fs_open(eventloop(), &file.f2, dest, O_WRONLY, 0, open_cb);
}
