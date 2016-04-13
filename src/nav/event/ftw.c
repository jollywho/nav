#include <malloc.h>
#include <string.h>
#include <uv.h>
#include "nav/event/ftw.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/event/event.h"

#define MAX_WAIT 30

typedef void (*recurse_cb)(const char *, struct stat *);
typedef struct {
  bool running;
  bool cancel;
  uint64_t before;
  off_t size;
  int count;
  int depth;
  FileItem *cur;
  TAILQ_HEAD(cont, FileItem) p;
} Ftw;
static Ftw ftw;

void ftw_init()
{
  TAILQ_INIT(&ftw.p);
  ftw.running = false;
}

void ftw_cleanup()
{
  //cancel items
}

void recurse(const char *path, recurse_cb fn)
{
  char buf[PATH_MAX], *p;
  struct dirent *d;
  struct stat st;
  DIR *dp;

  if (lstat(path, &st) == -1)
    return;

  fn(path, &st);

  if (!S_ISDIR(st.st_mode))
    return;

  if (!(dp = opendir(path)))
    log_err("FTW", "opendir failed %s", path);

  ftw.depth++;

  while ((d = readdir(dp))) {
    if (ftw.cancel) {
      log_err("FTW", "<|_CANCEL_|>");
      break;
    }

    uint64_t now = os_hrtime();
    if ((now - ftw.before)/1000000 > MAX_WAIT) {
      event_cycle_once();
      ftw.before = os_hrtime();
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

    recurse(buf, fn);
  }

  ftw.depth--;
  closedir(dp);
}

static void ftw_cb(const char *str, struct stat *sb)
{
  log_msg("FTW", "%s %d", str, ftw.depth);
  //log_msg("FTW", "%ld", sb->st_size);
  //log_msg("FTW", "[%ld]", f->size);
  ftw.size += sb->st_size;
  ftw.count++;

  if (ftw.depth > 0) {
    FileItem *cpy = malloc(sizeof(FileItem));
    cpy->src = strdup(str);
    cpy->dest = NULL;
    cpy->parent = ftw.cur;
    file_push(cpy);
  }
  else {
    TAILQ_REMOVE(&ftw.p, ftw.cur, ent);
    file_push(ftw.cur);
  }

  //onerror: pop item from ftw_queue
}

void ftw_cancel()
{
  ftw.cancel = true;
}

static void ftw_start()
{
  ftw.running = true;
  ftw.cancel = false;
  ftw.depth = 0;
  ftw.before = os_hrtime();

  ftw.cur = TAILQ_FIRST(&ftw.p);
  recurse(ftw.cur->src, ftw_cb);

  log_msg("FTW", "Finished");
  char buf[20];
  readable_fs(ftw.size, buf);

  //check for more entries
  ftw.running = false;
  ftw.cur = NULL;
  file_start();
}

void ftw_push(FileItem *item)
{
  TAILQ_INSERT_TAIL(&ftw.p, item, ent);
  if (!ftw.running)
    ftw_start();
}
