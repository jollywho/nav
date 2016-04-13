#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <uv.h>
#include "nav/nav.h"
#include "nav/log.h"
#include "nav/macros.h"
#include "nav/event/file.h"
#include "nav/event/event.h"
#include "nav/event/fs.h"
#include "nav/event/ftw.h"

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)
#define FFRESH O_CREAT|O_EXCL|O_WRONLY

static void write_cb(uv_fs_t *req);
static void open_cb(uv_fs_t *req);
static void mkdir_cb(uv_fs_t *req);
static void do_stat(File *file, const char *path, struct stat *sb);

struct File {
  uv_fs_t w1;         //writer
  uv_fs_t mk1, mk2;   //mkdir, mkdir_ver
  uv_fs_t f1, f2, f3; //src,dest,dest_ver
  uv_fs_t c1, c2;     //close
  uv_fs_t m1, m2;     //scan_ver, scan_mkdir
  int opencnt;        //ref_count src,dest
  uint64_t len;       //file length
  uint64_t offset;    //write offset
  uint64_t blk;       //write blk size
  uv_file u1, u2;
  struct stat s1, s2; //file stat
  char *src;
  char *dest;
  File *file;
  FileRet fr;
};

static void file_cleanup(void **args)
{
  File *file = args[0];
  free(file);
}

static void file_finish(File *file)
{
  log_msg("FILE", "close");
  //TODO: chmod
  if (file->fr.cb)
    file->fr.cb(file->fr.arg);

  free(file->src);
  free(file->dest);
  uv_fs_req_cleanup(&file->f1);
  uv_fs_req_cleanup(&file->f2);
  uv_fs_req_cleanup(&file->f3);
  uv_fs_req_cleanup(&file->w1);
  uv_fs_req_cleanup(&file->c1);
  uv_fs_req_cleanup(&file->c2);
  CREATE_EVENT(eventq(), file_cleanup, 1, file);
}

static void close_cb(uv_fs_t *req)
{
  File *file = req->data;
  if (req->result < 0)
    log_err("FILE", "close_cb: |%s|", uv_strerror(req->result));

  file->opencnt--;
  if (file->opencnt == 0)
    file_finish(file);
}

static void file_close(File *file)
{
  file->c1.data = file->c2.data = file;
  uv_fs_close(eventloop(), &file->c1, file->u1, close_cb);
  uv_fs_close(eventloop(), &file->c2, file->u2, close_cb);
}


static int dest_ver(const char *base, const char *other, size_t baselen)
{
  int version = 0;
  const char *p;

  if (!strncmp(base, other, baselen) && other[baselen] == '_') {
    for (p = &other[baselen+1]; ISDIGIT(*p); ++p)
      version = version * 10 + *p - '0';
    if (p[0])
      version = 0;
  }
  return version;
}

static void scan_cb(uv_fs_t *req)
{
  if (req->result < 0) {
    log_err("FILE", "scan_cb: |%s| ", uv_strerror(req->result));
    exit(1);
  }
  File *file = req->data;
  char *base = basename(file->src);
  char *dir = dirname(file->src);
  int baselen = strlen(base);
  int max_ver = 0;

  uv_dirent_t dent;
  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    int ver = dest_ver(base, dent.name, baselen);
    if (ver > max_ver)
      max_ver = ver;
  }

  free(file->dest);
  asprintf(&file->dest, "%s/%s_%d", dir, base, max_ver+1);
  log_msg("FILE", "NEWVER: %s", file->dest);
  uv_fs_req_cleanup(req);

  if (S_ISDIR(file->s1.st_mode)) {
    file->mk2.data = file;
    uv_fs_mkdir(eventloop(), &file->mk2,
        file->dest, 0644, mkdir_cb);
  }
  else {
    file->f3.data = file;
    uv_fs_open(eventloop(), &file->f3, file->dest, FFRESH, 0644, open_cb);
  }
}

static void find_dest_name(File *file, char *path, uv_fs_t *fs)
{
  fs->data = file;
  uv_fs_scandir(eventloop(), fs, dirname(file->dest), 0, scan_cb);
}

static void try_sendfile(File *file)
{
  while(1) {
    int ret = uv_fs_sendfile(eventloop(),
        &file->w1,
        file->u2, file->u1,
        file->offset, file->blk, write_cb);
    if (ret > -1)
      return;
    if (ret == UV_EINTR || ret == UV_EAGAIN)
      continue;
  }
}

static void write_cb(uv_fs_t *req)
{
  File *file = req->data;
  if (req->result < 0) {
    log_err("FILE", "write_cb: |%s|", uv_strerror(req->result));
    file_close(file);
  }

  file->offset += req->result;

  if (file->offset < file->len)
    try_sendfile(file);
  else
    file_close(file);
}

static void mkdir_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "mkdir_cb: |%s|", uv_strerror(req->result));
  File *file = req->data;

  uv_fs_req_cleanup(req);

  if (req->result == UV_EEXIST) {
    log_err("FILE", "file exists: %s", req->path);
    find_dest_name(file, file->dest, &file->m2);
    return;
  }
  //  scandir(src)
  //  file_copy(cons(src, ent->name), cons(dest, ent->name));
  file_finish(file);
}

static void open_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "open_cb: |%s|", uv_strerror(req->result));
  File *file = req->data;
  log_msg("FILE", "open-cb");

  if (req == &file->f2 || req == &file->f3) {
    if (req->result == UV_EEXIST) {
      log_err("FILE", "file exists: %s", req->path);
      find_dest_name(file, file->dest, &file->m1);
      return;
    }
    file->u2 = req->result;
  }

  file->opencnt++;

  if (req == &file->f1) {
    file->u1 = req->result;
    do_stat(file, file->dest, &file->s2);
    return;
  }

  if (file->opencnt == 2) {
    file->w1.data = file;
    try_sendfile(file);
  }
}

static void do_stat(File *file, const char *path, struct stat *sb)
{
  if (stat(path, sb) == -1)
    log_err("FILE", "do_stat: |%s|", strerror(errno));

  if (sb == &file->s1) {
    if (S_ISDIR(sb->st_mode)) {
      file->mk1.data = file;
      SWAP_ALLOC_PTR(file->dest, conspath(file->dest, basename(file->src)));
      uv_fs_mkdir(eventloop(), &file->mk1,
          file->dest, sb->st_mode, mkdir_cb);
      return;
    }
    file->len = sb->st_size;
    file->blk = MAX(sb->st_blksize, 4096);
    log_msg("FILE", "use %ld for %ld", file->blk, file->len);
    file->f1.data = file;
    uv_fs_open(eventloop(), &file->f1, file->src, O_RDONLY, 0, open_cb);
    return;
  }

  if (sb == &file->s2) {
    if (S_ISDIR(sb->st_mode))
      SWAP_ALLOC_PTR(file->dest, conspath(file->dest, basename(file->src)));

    log_err("FILE", "open: %s", file->dest);
    file->f2.data = file;
    uv_fs_open(eventloop(), &file->f2, file->dest, FFRESH, 0644, open_cb);
  }
}

void file_copy(const char *src, const char *dest, FileRet fr)
{
  log_msg("FILE", "copy |%s|%s|", src, dest);
  File *file = malloc(sizeof(File));
  memset(file, 0, sizeof(File));
  file->src = strdup(src);
  file->dest = strdup(dest);
  file->fr = fr;

  ftw_start(src, (FileRet){});
  do_stat(file, file->src, &file->s1);
}
