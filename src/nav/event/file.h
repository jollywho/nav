#ifndef FN_FILE_H
#define FN_FILE_H

#include <uv.h>
#include "nav/lib/sys_queue.h"
#include "nav/plugins/plugin.h"

typedef struct FileItem FileItem;
struct FileItem {
  TAILQ_ENTRY(FileItem) ent;
  Buffer *owner;
  char *src;
  char *dest;
  FileItem *parent;
};

void file_init();
void file_cleanup();
void file_copy(const char *src, const char *dest, Buffer *);
void file_push(FileItem *item, uint64_t len);
void file_start();
void file_cancel(Buffer *);
long file_progress();

#endif
