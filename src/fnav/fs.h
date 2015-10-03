#ifndef FN_CORE_FS_H
#define FN_CORE_FS_H

#include "fnav/loop.h"

typedef struct {
  Job job;
  uv_fs_event_t watcher;
  Loop loop;
  bool cancel;
} FS_handle;

typedef void(*req_cb)();

typedef struct {
  FS_handle *fs_h;

  uv_stat_t uv_stat;
  uv_fs_t uv_fs; //data->req_handle

  req_cb scan_cb;
  req_cb close_cb;

  String req_name;
  fn_rec *rec;
} FS_req ;

FS_handle* fs_init(Cntlr *c, fn_handle *h, cntlr_cb read_cb);
void fs_open(FS_handle *h, String dir);
bool isdir(fn_rec *rec);
String fs_parent_dir(String path);

#endif
