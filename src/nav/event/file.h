#ifndef NV_FILE_H
#define NV_FILE_H

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
  char *src;
  char *dest;
  FileItem *parent;
  int refs;
};

typedef struct FileGroup FileGroup;
struct FileGroup {
  TAILQ_HEAD(cont, FileItem) p;
  TAILQ_ENTRY(FileGroup) ent;
  Buffer *owner;
  int flags;       //F_MOVE, F_COPY etc
  uint64_t tsize;  //size to write
  uint64_t wsize;  //size written
};

void file_init();
void file_cleanup();
void file_move_str(char *src, char *dst, Buffer *);
void file_copy(varg_T, char *dest, Buffer *);
void file_move(varg_T, char *dest, Buffer *);
void file_remove(varg_T, Buffer *);
void file_push(FileGroup *fg);
void file_start();
void file_cancel(Buffer *);
long file_progress();

#endif
