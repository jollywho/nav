//file tree walk
#include <malloc.h>
#include <string.h>
#include <uv.h>
#include "nav/event/ftw.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/event/event.h"

typedef struct {
  bool running;
  bool cancel;
  uint64_t before;
  int count;
  int depth;
  FileItem  *cur;
  FileGroup *fg;
  TAILQ_HEAD(Groups, FileGroup) p;
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

static void add_ent(const char *str, struct stat *sb, int isdir)
{
  log_msg("FTW", "add ent %s %d", str, ftw.depth);
  ftw.count++;
  ftw.fg->tsize += sb->st_size;

  if (ftw.depth == 0)
    return;

  FileItem *item = calloc(1, sizeof(FileItem));
  item->src = strdup(str);
  item->parent = ftw.cur;
  ftw.cur->refs++;
  TAILQ_INSERT_TAIL(&ftw.fg->p, item, ent);

  if (isdir)
    ftw.cur = item;

  //TODO: onerror, pop item from ftw_queue
}

static void do_ftw(const char *path)
{
  char buf[PATH_MAX], *p;
  struct dirent *d;
  struct stat st;
  DIR *dp;

  if (lstat(path, &st) == -1)
    return;

  int isdir = S_ISDIR(st.st_mode);

  add_ent(path, &st, isdir);

  if (!isdir)
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

    do_ftw(buf);
  }

  ftw.depth--;
  ftw.cur = ftw.cur->parent;
  closedir(dp);
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

  ftw.fg = TAILQ_FIRST(&ftw.p);
  ftw.cur = TAILQ_FIRST(&ftw.fg->p);
  do_ftw(ftw.cur->src);
  TAILQ_REMOVE(&ftw.p, ftw.fg, ent);

  log_msg("FTW", "Finished");

  //check for more entries
  ftw.running = false;
  file_push(ftw.fg);
}

FileItem* ftw_new(char *src, char *dest)
{
  FileItem *item = malloc(sizeof(FileItem));
  item->src = strdup(src);
  item->dest = dest ? strdup(dest) : NULL;
  item->parent = NULL;
  item->refs = 0;
  return item;
}

void ftw_push_move(char *src, char *dest, FileGroup *fg)
{
  log_msg("FTW", "pushed_move");
  FileItem *item = ftw_new(src, dest);
  TAILQ_INSERT_TAIL(&fg->p, item, ent);
  fg->flags = F_MOVE|F_VERSIONED;

  //NOTE: copying across devices will fail and be readded as copy
  file_push(fg);
}

void ftw_push_copy(char *src, char *dest, FileGroup *fg)
{
  log_msg("FTW", "pushed_copy");
  FileItem *item = ftw_new(src, dest);
  TAILQ_INSERT_TAIL(&fg->p, item, ent);
  TAILQ_INSERT_TAIL(&ftw.p, fg, ent);

  fg->flags |= F_COPY|F_VERSIONED;
  if (!ftw.running)
    ftw_start();
}

void ftw_push_remove(char *src, FileGroup *fg)
{
  log_msg("FTW", "pushed_remove");
  FileItem *item = ftw_new(src, NULL);
  TAILQ_INSERT_TAIL(&fg->p, item, ent);
  TAILQ_INSERT_TAIL(&ftw.p, fg, ent);

  fg->flags = F_UNLINK;
  if (!ftw.running)
    ftw_start();
}

//enqueue but don't start
void ftw_add_again(char *src, char *dst, FileGroup *fg)
{
  FileItem *item = ftw_new(src, dst);
  TAILQ_INSERT_TAIL(&fg->p, item, ent);
  TAILQ_INSERT_HEAD(&ftw.p, fg, ent);
  fg->flags |= F_COPY;
}

void ftw_retry()
{
  if (!ftw.running)
    ftw_start();
}

void ftw_add(char *src, char *dst, FileGroup *fg)
{
  log_msg("FILE", "ftw add |%s|%s|", src, dst);
  if (fg->flags == F_MOVE)
    ftw_push_move(src, dst, fg);
  else if (fg->flags == F_UNLINK)
    ftw_push_remove(src, fg);
  else
    ftw_push_copy(src, dst, fg);
}
