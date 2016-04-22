#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include "nav/nav.h"
#include "nav/log.h"
#include "nav/macros.h"
#include "nav/util.h"
#include "nav/event/file.h"
#include "nav/event/event.h"
#include "nav/event/fs.h"
#include "nav/event/ftw.h"
#include "nav/tui/buffer.h"

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)
#define FFRESH O_CREAT|O_EXCL|O_WRONLY
#define MAX_WAIT 60

static void write_cb(uv_fs_t *);
static void do_stat(const char *, struct stat *);

typedef struct File File;
struct File {
  uv_fs_t w1;         //writer
  uv_fs_t f1, f2;     //src, dest
  uv_fs_t c1, c2;     //close
  uv_fs_t m1;         //version scan
  uv_file u1, u2;     //file handles
  int opencnt;        //ref_count files1,2
  bool running;
  uint64_t len;       //file length
  uint64_t offset;    //write offset
  uint64_t blk;       //write blk size
  uint64_t tsize;     //size of queue
  uint64_t wsize;     //written in queue
  uint64_t before;    //last eventloop time
  struct stat s1, s2; //file stat
  FileItem *cur;      //current item
  FileItem *curp;     //parent  item
  TAILQ_HEAD(cont, FileItem) p;
};
static File file;
static char target[PATH_MAX];

void file_init()
{
  file.running = false;
  TAILQ_INIT(&file.p);
  ftw_init();
}

void file_cleanup()
{
  //prompt to cancel queued items.
  //cancel items
  ftw_cleanup();
}

static void clear_fileitem(FileItem *item)
{
  free(item->src);
  free(item->dest);
  free(item);
}

static void cleanup_fileitems(FileItem *cur, FileItem *p)
{
  if (BITMASK_CHECK(F_MOVE, file.cur->flags))
    return clear_fileitem(cur);

  if (S_ISDIR(file.s1.st_mode)) {
    file.curp = cur;
    return;
  }

  if (p && (cur->parent != p || TAILQ_EMPTY(&file.p)))
    clear_fileitem(p);

  file.curp = cur->parent;
  clear_fileitem(cur);
}

static void file_stop()
{
  log_msg("FILE", "close");
  //TODO: chmod

  Buffer *owner = file.cur->owner;

  TAILQ_REMOVE(&file.p, file.cur, ent);
  cleanup_fileitems(file.cur, file.curp);

  if (TAILQ_EMPTY(&file.p))
    file.tsize = file.wsize = 0;

  memset(&file.s1, 0, sizeof(struct stat));
  memset(&file.s2, 0, sizeof(struct stat));

  uv_fs_req_cleanup(&file.w1);
  uv_fs_req_cleanup(&file.f1);
  uv_fs_req_cleanup(&file.f2);
  uv_fs_req_cleanup(&file.c1);
  uv_fs_req_cleanup(&file.c2);

  file.running = false;
  CREATE_EVENT(eventq(), file_start, 0, NULL);
  buf_update_progress(owner, file_progress());
}

static void close_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "close_cb: |%s|", uv_strerror(req->result));

  file.opencnt--;
  if (file.opencnt == 0)
    file_stop();
}

static void file_close()
{
  uv_fs_close(eventloop(), &file.c1, file.u1, close_cb);
  uv_fs_close(eventloop(), &file.c2, file.u2, close_cb);
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
  char *path = strdup(file.cur->src);
  char *base = basename(path);
  char *dir = dirname(path);
  int baselen = strlen(base);
  int max_ver = 0;

  uv_dirent_t dent;
  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    int ver = dest_ver(base, dent.name, baselen);
    if (ver > max_ver)
      max_ver = ver;
  }

  free(file.cur->dest);
  asprintf(&file.cur->dest, "%s/%s_%d", dir, base, max_ver+1);
  free(path);
  log_msg("FILE", "NEWVER: %s", file.cur->dest);

  uv_fs_req_cleanup(req);
  do_stat(file.cur->dest, &file.s2);
}

static void find_dest_name()
{
  uv_fs_scandir(eventloop(), &file.m1, dirname(file.cur->dest), 0, scan_cb);
}

static void try_sendfile()
{
  while(1) {
    int ret = uv_fs_sendfile(eventloop(),
        &file.w1,
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
    log_err("FILE", "write_cb: |%s|", uv_strerror(req->result));
    return file_close();
  }

  file.offset += req->result;
  file.wsize += req->result;

  uint64_t now = os_hrtime();
  if ((now - file.before)/1000000 > MAX_WAIT) {
    buf_update_progress(file.cur->owner, file_progress());
    file.before = os_hrtime();
  }

  if (file.offset < file.len)
    try_sendfile();
  else
    file_close();
}

static void open_cb(uv_fs_t *req)
{
  log_msg("FILE", "open_cb");
  if (req->result < 0) {
    log_err("FILE", "open_cb: |%s|", uv_strerror(req->result));
    return;
  }

  file.opencnt++;

  if (req == &file.f2) {
    log_msg("FILE", "OPENED %s", req->path);
    file.u2 = req->result;
  }

  if (req == &file.f1) {
    log_msg("FILE", "OPENED %s", req->path);
    file.u1 = req->result;
    file.len = file.s1.st_size;
    file.blk = MAX(file.s1.st_blksize, 4096);
  }

  if (file.opencnt == 2)
    try_sendfile();
}

static void do_link(const char *oldpath, const char *newpath)
{
  log_err("FILE", "do_link");
  log_err("FILE", "%s %s", oldpath, newpath);

  int ret = link(oldpath, newpath);
  if (ret < 0)
    log_err("FILE", "%s", strerror(errno));

  if (errno == EXDEV) {
    log_err("FILE", "requeue as copy");
    return;
  }

  if (ret == 0) {
    unlink(oldpath);
    file_stop();
  }
}

static void get_cur_dest_name(const char *dest, FileItem *parent)
{
  if (BITMASK_CHECK(F_MOVE, file.cur->flags))
    return;
  log_err("FILE", "%s", dest);

  if (parent)
    dest = parent->dest;
  SWAP_ALLOC_PTR(file.cur->dest, conspath(dest, basename(file.cur->src)));
}

static void do_stat(const char *path, struct stat *sb)
{
  log_err("FILE", "do_stat");
  log_err("FILE", "%s", path);

  int ret = lstat(path, sb);
  if (ret < 0)
    log_err("FILE", "%s", strerror(errno));

  /* src exists, check dest */
  if (sb == &file.s1 && ret == 0) {
    get_cur_dest_name(file.cur->dest, file.cur->parent);
    return do_stat(file.cur->dest, &file.s2);
  }

  /* dest already exists */
  if (sb == &file.s2 && ret == 0) {
    return find_dest_name();
  }

  /* dest does not exist */
  if (sb == &file.s2 && errno == ENOENT) {

    if (BITMASK_CHECK(F_MOVE, file.cur->flags)) {
      do_link(file.cur->src, file.cur->dest);
      return;
    }
    if (S_ISDIR(file.s1.st_mode)) {
      mkdir(file.cur->dest, file.s1.st_mode);
      file_stop();
      return;
    }
    if (S_ISLNK(file.s1.st_mode)) {
      int r = readlink(file.cur->src, target, sizeof(target));
      target[r] = '\0';
      symlink(target, file.cur->dest);
      file_stop();
      return;
    }
    if (!S_ISREG(file.s1.st_mode))
      return;
    uv_fs_open(eventloop(), &file.f1, file.cur->src,  O_RDONLY, 0644, open_cb);
    uv_fs_open(eventloop(), &file.f2, file.cur->dest, FFRESH,   0644, open_cb);
  }
}

long file_progress()
{
  if (TAILQ_EMPTY(&file.p) || file.tsize < 1)
    return 0;

  long long sofar = file.wsize * 100;
  sofar /= file.tsize;
  if (sofar > 100)
    sofar = 100;
  return sofar;
}

void file_push(FileItem *item, uint64_t len)
{
  file.tsize += len;
  TAILQ_INSERT_TAIL(&file.p, item, ent);
}

void file_start()
{
  if (file.running || TAILQ_EMPTY(&file.p))
    return;

  file.running = true;
  file.cur = TAILQ_FIRST(&file.p);
  file.offset = 0;
  file.before = os_hrtime();
  do_stat(file.cur->src, &file.s1);
}

void file_cancel(Buffer *owner)
{
  //search for owner in queue
  //cancel matches
  //  F_COPY: unlink dest if opened
  ftw_cancel();
}

void file_move_str(char *src, char *dest, Buffer *owner)
{
  log_msg("FILE", "move_str |%s|%s|", src, dest);
  ftw_add(src, dest, owner, F_MOVE);
}

void file_move(varg_T args, char *dest, Buffer *owner)
{
  log_msg("FILE", "move |%d|%s|", args.argc, dest);
  for (int i = 0; i < args.argc; i++) {
    conspath_buf(target, dest, basename(args.argv[i]));
    ftw_add(args.argv[i], target, owner, F_MOVE);
  }
  //
  //**flags**
  //dest overwrite [edit (rename) default]
  //  unlink dest if exists.
  //dest version   [mv,cp default]
  //  increment version if exists.
}

void file_copy(varg_T args, char *dest, Buffer *owner)
{
  log_msg("FILE", "copy |%d|%s|", args.argc, dest);
  for (int i = 0; i < args.argc; i++) {
    ftw_add(args.argv[i], dest, owner, F_COPY);
  }
}
