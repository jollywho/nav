#ifndef FN_EVENT_FS_H
#define FN_EVENT_FS_H

#include "fnav/event/event.h"
#include "fnav/table.h"

typedef struct fentry fentry;
typedef struct fn_fs fn_fs;

struct fn_fs {
  char *path;

  fn_handle *hndl;
  uv_fs_t uv_fs;  /* readonly stat */
  char *readkey;
  fentry *ent;

  void *data;
  bool running;
  argv_callback open_cb;
  argv_callback stat_cb;
  UT_hash_handle hh;
};

fn_fs* fs_init(fn_handle *hndl);
void fs_cleanup(fn_fs *fs);
void fs_open(fn_fs *fs, const char *);
void fs_close(fn_fs *fs);
void fs_read(fn_fs *fs, const char *);

void fs_fastreq(fn_fs *fs);
bool fs_blocking(fn_fs *fs);

bool isdir(const char *);
char* fs_expand_path(const char *);
char* fs_parent_dir(char *);
void* fs_vt_stat_resolv(fn_rec *rec, const char *);
long fs_vt_sz_resolv(const char *key);
char* conspath(const char *, const char *);

char* fs_current_dir();

#endif
