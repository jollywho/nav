#ifndef FN_FILE_H
#define FN_FILE_H

#include "nav/lib/sys_queue.h"

typedef struct File File;
typedef void (*file_copy_cb)(void *);
typedef struct {
  file_copy_cb cb;
  void *arg;
} FileRet;

typedef struct FileItem FileItem;
struct FileItem {
  TAILQ_ENTRY(FileItem) ent;
  char *src;
  char *dest;
  FileItem *parent;
};

void file_init();
void file_cleanup();
void file_copy(const char *src, const char *dest, FileRet fr);
void file_push(FileItem *item);
void file_start();

#endif
