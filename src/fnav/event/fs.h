#ifndef FN_EVENT_FS_H
#define FN_EVENT_FS_H

#include "fnav/event/loop.h"
#include "fnav/table.h"

typedef struct {
  uv_fs_event_t watcher;
  uv_timer_t watcher_timer;
  fn_handle *hndl;
  Loop loop;
  bool cancel;
  bool running;
  bool queued;
  uint64_t last_ran;
  String path;
} FS_handle;

typedef struct {
  FS_handle *fs_h;

  uv_stat_t uv_stat;
  uv_fs_t uv_fs; //data->req_handle

  void *data;
  String req_name;
} FS_req;

FS_handle* fs_init(fn_handle *h);
void fs_cleanup(FS_handle *fsh);
void fs_open(FS_handle *h, const String dir);
void fs_close(FS_handle *h);
bool isdir(String path);
String fs_expand_path(String path);
String fs_parent_dir(const String path);
void fs_async_open(FS_handle *fsh, Cntlr *cntlr, String path);
void* fs_vt_stat_resolv(fn_rec *rec, String key);

#endif
