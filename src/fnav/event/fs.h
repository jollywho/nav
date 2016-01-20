#ifndef FN_EVENT_FS_H
#define FN_EVENT_FS_H

#include "fnav/event/loop.h"
#include "fnav/table.h"

typedef struct fentry fentry;
typedef struct fn_fs fn_fs;

struct fn_fs {
  String path;

  fn_handle *hndl;

  void *data;
  argv_callback open_cb;
  argv_callback stat_cb;
  UT_hash_handle hh;
};

fn_fs* fs_init(fn_handle *h);
void fs_cleanup(fn_fs *fsh);
void fs_open(fn_fs *h, String dir);
void fs_close(fn_fs *h);
bool isdir(String path);
String fs_expand_path(String path);
String fs_parent_dir(const String path);
void fs_async_open(fn_fs *fsh, Cntlr *cntlr, String path);
void* fs_vt_stat_resolv(fn_rec *rec, String key);
String conspath(const char *str1, const char *str2);

#endif
