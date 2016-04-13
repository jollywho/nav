#include <string.h>
#include <uv.h>
#include "nav/event/ftw.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/event/event.h"

typedef struct {
  /* files hash: path,stat */
  uint64_t before;
  off_t size;
  int count;
  bool cancel;
  FileRet ret;
} Ftw;

#define MAX_WAIT 30

static int cancel;

//copy -> ftw -> ftw_cb -> copy_cb

typedef void (*recurse_cb)(const char *, struct stat *, Ftw *f);

void recurse(const char *path, recurse_cb fn, Ftw *f)
{
  char buf[PATH_MAX], *p;
  struct dirent *d;
  struct stat st;
  DIR *dp;

  if (lstat(path, &st) == -1 || !S_ISDIR(st.st_mode))
    return fn(path, &st, f);

  if (!(dp = opendir(path)))
    log_err("FTW", "opendir failed %s", path);

  while ((d = readdir(dp))) {

    if (f->cancel || cancel) {
      log_err("FTW", "<|_CANCEL_|>");
      break;
    }
    uint64_t now = os_hrtime();
    if ((now - f->before)/1000000 > MAX_WAIT) {
      event_cycle_once();
      f->before = os_hrtime();
    }

    if (strcmp(d->d_name, ".") == 0 ||
        strcmp(d->d_name, "..") == 0)
      continue;

    strncpy(buf, path, sizeof(buf));
    p = strrchr(buf, '\0');

    /* remove all trailing slashes */
    while (--p >= buf && *p == '/')
      *p ='\0';

    strcat(buf, "/");
    strcat(buf, d->d_name);

    recurse(buf, fn, f);
  }

  closedir(dp);
}

static void ftw_cb(const char *str, struct stat *sb, Ftw *f)
{
  //log_msg("FTW", "%s", str);
  //log_msg("FTW", "%ld", sb->st_size);
  //log_msg("FTW", "[%ld]", f->size);
  f->size += sb->st_size;
  f->count++;

  //push item to file_queue
  //if depth == 0, pop item from ftw_queue
  //onerror: pop item from ftw_queue
}

void ftw_cancel()
{
  cancel = 1;
}

void ftw_start(const char *dirpath, FileRet fr)
{
  cancel = 0;
  uint64_t before = os_hrtime();
  Ftw f = {before,0,0,0,{0,0}};
  recurse(dirpath, ftw_cb, &f);
  log_msg("FTW", "Finished");

  uint64_t now = os_hrtime();
  char buf[20];
  uint64_t ran = (now - before);
  readable_fs(f.size, buf);
  log_msg("FTW", "Size: %s, Count: %d Ran: %ld", buf, f.count, ran);
}
