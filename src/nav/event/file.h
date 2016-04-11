#ifndef FN_FILE_H
#define FN_FILE_H

typedef struct File File;
typedef void (*file_copy_cb)(void *);
typedef struct {
  file_copy_cb cb;
  void *arg;
} FileRet;

void file_copy(const char *src, const char *dest, FileRet fr);

#endif
