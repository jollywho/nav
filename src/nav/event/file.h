#ifndef FN_FILE_H
#define FN_FILE_H

#include <uv.h>
#include "nav/lib/sys_queue.h"
#include "nav/plugins/plugin.h"

#define F_COPY       1
#define F_MOVE       2
#define F_VERSIONED  4
#define F_OVERWRITE  8
#define F_UNLINK     16
#define F_ERROR      32

typedef struct FileItem FileItem;
struct FileItem {
  TAILQ_ENTRY(FileItem) ent;
  Buffer *owner;
  int flags;
  char *src;
  char *dest;
  FileItem *parent;
};

void file_init();
void file_cleanup();
void file_move_str(char *src, char *dst, Buffer *);
void file_copy(varg_T, char *dest, Buffer *);
void file_move(varg_T, char *dest, Buffer *);
void file_push(FileItem *item, uint64_t len);
void file_start();
void file_cancel(Buffer *);
long file_progress();

#endif
