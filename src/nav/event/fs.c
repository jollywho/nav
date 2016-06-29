#include <time.h>
#include <malloc.h>
#include <sys/time.h>
#include <libgen.h>
#include <wordexp.h>
#include <unistd.h>

#include "nav/event/fs.h"
#include "nav/model.h"
#include "nav/event/event.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/info.h"

static void fs_close_req(fentry *);
static void fs_reopen(fentry *);
static void stat_cb(uv_fs_t *);
static void fs_flush_stream(fentry *);
static void watch_cb(uv_fs_event_t *, const char *, int, int);

#define MAX_WAIT 1000

struct fentry {
  char *key;
  uv_fs_event_t watcher;
  uv_fs_t uv_fs;
  uv_timer_t watcher_timer;
  bool running;
  bool flush;
  bool cancel;
  bool reopen;
  uint64_t before;
  int refs;
  nv_fs *listeners;
  UT_hash_handle hh;
};

typedef struct {
  char *key;
  time_t ctimesec;
  UT_hash_handle hh;
} cachedir;

static fentry   *ent_tbl;
static cachedir *cache_tbl;

static fentry* fs_mux(nv_fs *fs)
{
  fentry *ent = NULL;
  HASH_FIND_STR(ent_tbl, fs->path, ent);
  if (!ent) {
    ent = calloc(1, sizeof(fentry));
    ent->key = strdup(fs->path);
    ent->refs = 2;
    uv_fs_event_init(eventloop(), &ent->watcher);
    uv_timer_init(eventloop(), &ent->watcher_timer);
    ent->watcher.data = ent;
    ent->watcher_timer.data = ent;
    ent->uv_fs.data = ent;
    HASH_ADD_STR(ent_tbl, key, ent);
  }

  nv_fs *find = NULL;
  HASH_FIND_PTR(ent->listeners, fs->path, find);
  if (!find)
    HASH_ADD_STR(ent->listeners, path, fs);

  fs->ent = ent;
  return ent;
}

static void del_ent(uv_handle_t *hndl)
{
  fentry *ent = hndl->data;
  ent->refs--;
  if (ent->refs < 1) {
    free(ent->key);
    free(ent);
  }
}

static void fs_demux(nv_fs *fs)
{
  log_msg("FS", "fs_demux");
  if (!fs->ent)
    return;
  fentry *ent = fs->ent;

  HASH_DEL(ent->listeners, fs);

  if (HASH_COUNT(ent->listeners) < 1) {
    HASH_DEL(ent_tbl, ent);
    uv_timer_stop(&ent->watcher_timer);
    uv_fs_event_stop(&ent->watcher);
    uv_fs_req_cleanup(&ent->uv_fs);
    uv_handle_t *hw = (uv_handle_t*)&ent->watcher;
    uv_handle_t *ht = (uv_handle_t*)&ent->watcher_timer;
    uv_close(hw, del_ent);
    uv_close(ht, del_ent);
  }

  free(fs->path);
  fs->ent = NULL;
}

nv_fs* fs_init(Handle *hndl)
{
  log_msg("FS", "init");
  nv_fs *fs = malloc(sizeof(nv_fs));
  memset(fs, 0, sizeof(nv_fs));
  fs->hndl = hndl;
  return fs;
}

void fs_cleanup(nv_fs *fs)
{
  log_msg("FS", "cleanup");
  fs_close(fs);
  free(fs);
}

void fs_clr_cache(char *path)
{
  cachedir *cache;
  HASH_FIND_STR(cache_tbl, path, cache);
  if (cache)
    cache->ctimesec = -1;
}

void fs_clr_all_cache()
{
  cachedir *it, *tmp;
  HASH_ITER(hh, cache_tbl, it, tmp) {
    HASH_DEL(cache_tbl, it);
    free(it->key);
    free(it);
  }
}

char* conspath(const char *str1, const char *str2)
{
  char *result;
  if (strcmp(str1, "/") == 0)
    asprintf(&result, "/%s", str2);
  else
    asprintf(&result, "%s/%s", str1, str2);
  return result;
}

char* fs_parent_dir(char *path)
{
  log_msg("FS", "<<PARENT OF>>: %s", path);
  return dirname(path);
}

char* fs_expand_path(const char *path)
{
  wordexp_t p;
  char *newpath = NULL;
  if (!wordexp(path, &p, 0)) {
    newpath = strdup(p.we_wordv[0]);
    wordfree(&p);
    return newpath;
  }
  return strdup(path);
}

char* fs_current_dir()
{
  return get_current_dir_name();
}

char* valid_full_path(char *base, char *path)
{
  if (!path)
    return strdup(base);

  char *dir = fs_expand_path(path);
  if (path[0] == '@') {
    char *tmp = mark_path(dir);
    if (tmp)
      SWAP_ALLOC_PTR(dir, strdup(tmp));
  }
  if (dir[0] != '/')
    SWAP_ALLOC_PTR(dir, conspath(base, dir));

  char *valid = realpath(dir, NULL);
  if (!valid) {
    free(dir);
    return NULL;
  }
  SWAP_ALLOC_PTR(dir, valid);
  return dir;
}

static void stat_cleanup(void **args)
{
  nv_fs *fs = args[0];
  free(args[1]);
  free(fs->readkey);
  uv_fs_req_cleanup(&fs->uv_fs);
  fs->running = false;
}

static void stat_read_cb(uv_fs_t *req)
{
  log_msg("FS", "stat_read_cb");
  nv_fs *fs = req->data;
  if (req->result < 0) {
    log_err("FILE", "stat_read_cb: |%s|", uv_strerror(req->result));
    CREATE_EVENT(eventq(), fs->stat_cb, 3, fs->data, fs->readkey, 0);
    CREATE_EVENT(eventq(), stat_cleanup, 2, fs, NULL);
    return;
  }

  char *path = realpath(fs->readkey, NULL);
  log_msg("FS", "req_stat_cb %s", req->path);

  CREATE_EVENT(eventq(), fs->stat_cb, 3, fs->data, path, &req->statbuf);
  CREATE_EVENT(eventq(), stat_cleanup, 2, fs, path);
}

const char* file_ext(const char *filename)
{
  const char *d = strrchr(filename, '.');
  return (d != NULL) ? d + 1 : "";
}

const char* file_base(char *filename)
{
  char *d = strrchr(filename, '.');
  if (d)
    *d = '\0';
  return filename;
}

const char* stat_type(struct stat *sb)
{
  switch (sb->st_mode & S_IFMT) {
    case S_IFDIR:  return "directory";
    case S_IFLNK:  return "symbolic";
    case S_IFREG:  return "regular";
    case S_IFBLK:  return "block";
    case S_IFIFO:  return "fifo";
    case S_IFSOCK: return "block";
    case S_IFCHR:  return "character";
    default:
      return "";
  }
}

bool isrecdir(TblRec *rec)
{
  struct stat *st = (struct stat*)rec_fld(rec, "stat");
  return (S_ISDIR(st->st_mode));
}

bool isreclnk(TblRec *rec)
{
  struct stat *st = (struct stat*)rec_fld(rec, "stat");
  return (S_ISLNK(st->st_mode));
}

bool isrecreg(TblRec *rec)
{
  struct stat *st = (struct stat*)rec_fld(rec, "stat");
  return (S_ISREG(st->st_mode));
}

time_t rec_ctime(TblRec *rec)
{
  struct stat *stat = (struct stat*)rec_fld(rec, "stat");
  return stat->st_ctim.tv_sec;
}

off_t rec_stsize(TblRec *rec)
{
  struct stat *stat = (struct stat*)rec_fld(rec, "stat");
  return stat->st_size;
}

mode_t rec_stmode(TblRec *rec)
{
  struct stat *stat = (struct stat*)rec_fld(rec, "stat");
  return stat->st_mode;
}

bool fs_vt_isdir_resolv(TblRec *rec)
{
  struct stat *st = (struct stat*)rec_fld(rec, "stat");
  return (S_ISDIR(st->st_mode));
}

void fs_signal_handle(void **data)
{
  log_msg("FS", "fs_signal_handle");
  fentry *ent = data[0];
  if (ent->cancel) {
    fs_flush_stream(ent);
    tbl_del_val("fm_files", "dir", ent->key);
    fs_clr_cache(ent->key);
  }

  nv_fs *it = NULL;
  for (it = ent->listeners; it != NULL; it = it->hh.next) {
    Handle *h = it->hndl;

    if (it->open_cb)
      it->open_cb(NULL);
    else
      model_recv(h->model);
  }
  ent->running = false;
  ent->flush = false;
  ent->reopen = false;
}

bool fs_blocking(nv_fs *fs)
{
  if (!fs->ent)
    return false;
  return fs->ent->running;
}

void fs_read(nv_fs *fs, const char *dir)
{
  log_msg("FS", "fs read %s", dir);
  if (fs->running)
    return;
  fs->running = true;
  fs->uv_fs.data = fs;
  fs->readkey = strdup(dir);
  uv_fs_stat(eventloop(), &fs->uv_fs, fs->readkey, stat_read_cb);
}

void fs_open(nv_fs *fs, const char *dir)
{
  log_msg("FS", "fs open %s", dir);
  fs->path = strdup(dir);
  fentry *ent = fs_mux(fs);

  if (!ent->running) {
    ent->running = true;

    uv_fs_stat(eventloop(), &ent->uv_fs, ent->key, stat_cb);
    uv_fs_event_start(&ent->watcher, watch_cb, ent->key, 1);
  }
}

void fs_close(nv_fs *fs)
{
  fs_demux(fs);
}

static int edit_trans_stat(trans_rec *r, const char *path, int upd)
{
  struct stat s;
  if (lstat(path, &s) == -1)
    return 1;

  struct stat *cstat = malloc(sizeof(struct stat));
  *cstat = s;
  edit_trans(r, "stat",     NULL,        cstat);
  return 0;
}

static void add_dir(const char *path, uv_stat_t stat)
{
  cachedir *cache;
  HASH_FIND_STR(cache_tbl, path, cache);
  if (!cache) {
    cache = malloc(sizeof(cachedir));
    cache->key = strdup(path);
    HASH_ADD_STR(cache_tbl, key, cache);
  }
  cache->ctimesec = stat.st_ctim.tv_sec;
}

void fs_cancel(nv_fs *fs)
{
  fs->ent->cancel = true;
  uv_cancel((uv_req_t*)&fs->ent->uv_fs);
}

static void fs_flush_stream(fentry *ent)
{
  nv_fs *it = NULL;
  for (it = ent->listeners; it != NULL; it = it->hh.next) {
    if (it->hndl->model)
      model_flush(it->hndl, ent->reopen);
  }
}

static void scan_cb(uv_fs_t *req)
{
  log_msg("FS", "--scan--");
  log_msg("FS", "path: %s", req->path);
  uv_dirent_t dent;
  fentry *ent = req->data;

  fs_flush_stream(ent);

  /* clear outdated records */
  tbl_del_val("fm_files", "dir", (char*)req->path);

  add_dir(req->path, req->statbuf);

  while (UV_EOF != uv_fs_scandir_next(req, &dent) && (!ent->cancel)) {
    int err = 0;
    trans_rec *r = mk_trans_rec(tbl_fld_count("fm_files"));
    edit_trans(r, "name", (char*)dent.name, NULL);
    edit_trans(r, "dir",  (char*)req->path, NULL);
    char *full = conspath(req->path, dent.name);
    edit_trans(r, "fullpath", (char*)full, NULL);

    err = edit_trans_stat(r, full, 0);
    free(full);

    if (err)
      clear_trans(r, 1);
    else
      CREATE_EVENT(eventq(), commit, 2, "fm_files", r);
  }
  uv_fs_req_cleanup(&ent->uv_fs);
  fs_close_req(ent);
}

static void stat_cb(uv_fs_t *req)
{
  log_msg("FS", "stat cb");
  bool doscan = false;
  fentry *ent = req->data;
  uv_stat_t stat = req->statbuf;
  if (req->result < 0) {
    log_err("FS", "stat_cb: |%s|", uv_strerror(req->result));
    ent->cancel = true;
    goto scandir;
  }

  if (!(doscan = S_ISDIR(stat.st_mode)))
    goto scandir;

  cachedir *cache;
  HASH_FIND_STR(cache_tbl, req->path, cache);

  if (!cache || ent->flush)
    goto scandir;

  if (!difftime(stat.st_ctim.tv_sec, cache->ctimesec)) {
    log_msg("FS", "STAT:NOP");
    doscan = false;
  }

scandir:
  ent->before = os_hrtime();
  uv_fs_req_cleanup(req);
  if (doscan)
    uv_fs_scandir(eventloop(), &ent->uv_fs, ent->key, 0, scan_cb);
  else
    fs_close_req(ent);
}

static void fs_reopen(fentry *ent)
{
  log_msg("FS", "--reopen--");
  uv_timer_stop(&ent->watcher_timer);
  uv_fs_event_stop(&ent->watcher);

  ent->running = true;
  ent->reopen = true;
  uv_fs_lstat(eventloop(), &ent->uv_fs, ent->key, stat_cb);
  uv_fs_event_start(&ent->watcher, watch_cb, ent->key, 1);
}

void fs_fastreq(nv_fs *fs)
{
  log_msg("FS", "fs_fastreq %d", fs->ent->running);
  fs->ent->before = MAX_WAIT;
  fs->ent->flush = true;
  if (!fs->ent->running)
    fs_reopen(fs->ent);
}

static void watch_timer_cb(uv_timer_t *handle)
{
  log_msg("FS", "--watch_timer--");
  fentry *ent = handle->data;
  if (!ent->running)
    fs_reopen(ent);
}

static void watch_cb(uv_fs_event_t *hndl, const char *fname, int events, int s)
{
  log_msg("FS", "--watch--");
  fentry *ent = hndl->data;

  uv_fs_event_stop(&ent->watcher);
  uv_timer_start(&ent->watcher_timer, watch_timer_cb, MAX_WAIT, MAX_WAIT);

  uint64_t now = os_hrtime();
  if ((now - ent->before)/1000000 > MAX_WAIT)
    watch_timer_cb(&ent->watcher_timer);
}

void fs_reload(char *path)
{
  fentry *ent = NULL;
  HASH_FIND_STR(ent_tbl, path, ent);
  if (!ent)
    return;
  ent->before = MAX_WAIT;
  ent->flush = true;
  uv_timer_start(&ent->watcher_timer, watch_timer_cb, 0, MAX_WAIT);
}

static void fs_close_req(fentry *ent)
{
  log_msg("FS", "reset %s", ent->key);
  CREATE_EVENT(eventq(), fs_signal_handle, 1, ent);
}
