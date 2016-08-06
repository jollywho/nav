//file copy and move, including queues and callback per request
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <libgen.h>
#include "nav/nav.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/event/file.h"
#include "nav/event/event.h"
#include "nav/event/fs.h"
#include "nav/event/ftw.h"
#include "nav/tui/buffer.h"

static void write_cb(uv_fs_t *);
static void do_stat(const char *, struct stat *);
static void file_check_update();
static void do_unlink(const char *, bool);

#define FFRESH O_CREAT|O_EXCL|O_WRONLY

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
  uint64_t before;    //last eventloop time
  struct stat s1, s2; //file stat
  FileItem  *cur;     //current fileitem
  FileGroup *fg;      //current filegroup
  TAILQ_HEAD(Groups, FileGroup) p;
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

static FileGroup* fg_new(Buffer *owner, int flags)
{
  FileGroup *fg = calloc(1, sizeof(FileGroup));
  TAILQ_INIT(&fg->p);
  fg->owner = owner;
  fg->flags = flags;
  return fg;
}

static void clear_fileitem(FileItem *item, bool isdir)
{
  if (item->refs != 0)
    return;

  do_unlink(item->src, isdir);

  FileItem *parent = item->parent;
  if (parent) {
    parent->refs--;
    clear_fileitem(parent, true);
  }
  free(item->src);
  free(item->dest);
  free(item);
}

static void try_clear_filegroup()
{
  if (TAILQ_EMPTY(&file.fg->p)) {
    TAILQ_REMOVE(&file.p, file.fg, ent);

    /* flush progress */
    file.before = MAX_WAIT;
    file_check_update();

    free(file.fg);
    file.fg = NULL;
  }
  else
    file_check_update();
}

static void file_stop()
{
  log_msg("FILE", "close");
  //TODO: chmod

  TAILQ_REMOVE(&file.fg->p, file.cur, ent);
  clear_fileitem(file.cur, S_ISDIR(file.s1.st_mode));
  try_clear_filegroup();

  memset(&file.s1, 0, sizeof(struct stat));
  memset(&file.s2, 0, sizeof(struct stat));

  uv_fs_req_cleanup(&file.w1);
  uv_fs_req_cleanup(&file.f1);
  uv_fs_req_cleanup(&file.f2);
  uv_fs_req_cleanup(&file.c1);
  uv_fs_req_cleanup(&file.c2);

  file.running = false;
  CREATE_EVENT(eventq(), file_start, 0, NULL);
}

static void file_retry_as_copy()
{
  log_msg("FILE", "file_retry_as_copy");

  char *src = file.cur->src;
  char *dst = dirname(file.cur->dest);
  log_err("FILE", "retry %s %s", src, dst);

  ftw_add_again(src, dst, fg_new(file.fg->owner, F_UNLINK));
  file_stop();
  ftw_retry();
}

static void close_cb(uv_fs_t *req)
{
  if (req->result < 0)
    log_err("FILE", "close_cb: |%s|", uv_strerror(req->result));

  file.opencnt--;
  if (file.opencnt == 0)
    file_stop();
}

static void do_unlink(const char *path, bool isdir)
{
  log_msg("FILE", "do_unlink");
  if (!BITMASK_CHECK(F_UNLINK, file.fg->flags))
    return;
  if (BITMASK_CHECK(F_ERROR, file.fg->flags))
    return;

  log_msg("FILE", "unlink dir? %d", isdir);
  if (isdir)
    rmdir(path);
  else
    unlink(path);
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
  char *base = strdup(basename(file.cur->dest));
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
  free(base);
  log_msg("FILE", "NEWVER: %s", file.cur->dest);

  uv_fs_req_cleanup(req);
  do_stat(file.cur->dest, &file.s2);
}

static void find_dest_name()
{
  char *dir = dirname(strdup(file.cur->dest));
  uv_fs_scandir(eventloop(), &file.m1, dir, 0, scan_cb);
  free(dir);
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

static void file_check_update()
{
  uint64_t now = os_hrtime();
  if ((now - file.before)/1000000 > MAX_WAIT) {
    buf_update_progress(file.fg->owner, file_progress());
    file.before = os_hrtime();
  }
}

static void write_cb(uv_fs_t *req)
{
  if (req->result < 0) {
    log_err("FILE", "write_cb: |%s|", uv_strerror(req->result));
    file.fg->flags |= F_ERROR;
    return file_close();
  }

  file.offset += req->result;
  file.fg->wsize += req->result;
  file_check_update();

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

static void mk_dest(const char *oldpath, const char *newpath)
{
  log_err("FILE", "mk_dest %s %s", oldpath, newpath);

  int ret;
  if (S_ISDIR(file.s1.st_mode))
    ret = rename(oldpath, newpath);
  else
    ret = link(oldpath, newpath);

  if (ret != 0) {
    log_err("FILE", "%s", strerror(errno));
    if (errno == EXDEV)
      return file_retry_as_copy();

    log_err("FILE", "unhandled error");
    file.fg->flags |= F_ERROR;
    return;
  }

  file.fg->flags |= F_UNLINK;
  do_unlink(file.cur->src, S_ISDIR(file.s1.st_mode));
  file_stop();
}

static void do_copy(const char *src, const char *dst)
{
  log_err("FILE", "do_copy");
  int r;
  switch (file.s1.st_mode & S_IFMT) {
    case S_IFDIR:
      mkdir(dst, file.s1.st_mode);
      file.fg->wsize += file.s1.st_size;
      file_stop();
      break;
    case S_IFLNK:
      r = readlink(src, target, sizeof(target));
      target[r] = '\0';
      symlink(target, dst);
      file.fg->wsize += file.s1.st_size;
      file_stop();
      break;
    case S_IFREG:
      uv_fs_open(eventloop(), &file.f1, src, O_RDONLY, 0644, open_cb);
      uv_fs_open(eventloop(), &file.f2, dst, FFRESH,   0644, open_cb);
      break;
  }
}

static void get_cur_dest_name(const char *dest)
{
  if (BITMASK_CHECK(F_MOVE, file.fg->flags))
    return;

  if (file.cur->parent)
    dest = file.cur->parent->dest;
  SWAP_ALLOC_PTR(file.cur->dest, conspath(dest, basename(file.cur->src)));
}

static void do_stat(const char *path, struct stat *sb)
{
  log_err("FILE", "do_stat");

  int ret = lstat(path, sb);
  if (ret < 0)
    log_err("FILE", "%s", strerror(errno));

  /* src exists, check dest */
  if (sb == &file.s1 && ret == 0) {
    get_cur_dest_name(file.cur->dest);
    return do_stat(file.cur->dest, &file.s2);
  }

  /* dest already exists */
  if (sb == &file.s2 && ret == 0)
    return find_dest_name();

  /* dest does not exist */
  if (sb == &file.s2 && errno == ENOENT) {

    if (BITMASK_CHECK(F_MOVE, file.fg->flags))
      return mk_dest(file.cur->src, file.cur->dest);

    do_copy(file.cur->src, file.cur->dest);
  }
}

long file_progress()
{
  if (TAILQ_EMPTY(&file.p) || file.fg->tsize < 1)
    return 0;

  long long sofar = file.fg->wsize * 100;
  sofar /= file.fg->tsize;
  if (sofar > 100)
    sofar = 100;
  return sofar;
}

void file_push(FileGroup *item)
{
  TAILQ_INSERT_TAIL(&file.p, item, ent);
  if (!file.running)
    file_start();
}

void file_start()
{
  if (file.running || TAILQ_EMPTY(&file.p))
    return;
  if (!file.running)
    file.before = os_hrtime();

  file.running = true;
  file.fg = TAILQ_FIRST(&file.p);
  file.cur = TAILQ_FIRST(&file.fg->p);
  file.offset = 0;
  do_stat(file.cur->src, &file.s1);
}

void file_cancel(Buffer *owner)
{
  //TODO:
  //search for owner in queue
  //cancel matches
  //  F_COPY: unlink dest if opened
  ftw_cancel();
}

void file_move_str(char *src, char *dest, Buffer *owner)
{
  log_msg("FILE", "move_str |%s|%s|", src, dest);
  ftw_add(src, dest, fg_new(owner, F_MOVE));
}

void file_move(varg_T args, char *dest, Buffer *owner)
{
  log_msg("FILE", "move |%d|%s|", args.argc, dest);
  for (int i = 0; i < args.argc; i++) {
    conspath_buf(target, dest, basename(args.argv[i]));
    ftw_add(args.argv[i], target, fg_new(owner, F_MOVE));
  }
}

void file_copy(varg_T args, char *dest, Buffer *owner)
{
  log_msg("FILE", "copy |%d|%s|", args.argc, dest);
  for (int i = 0; i < args.argc; i++)
    ftw_add(args.argv[i], dest, fg_new(owner, F_COPY));
}
