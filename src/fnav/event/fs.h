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
  String path;
} FS_handle;

typedef struct {
  FS_handle *fs_h;

  uv_stat_t uv_stat;
  uv_fs_t uv_fs; //data->req_handle

  String req_name;
} FS_req;

FS_handle* fs_init(fn_handle *h);
void fs_cleanup(FS_handle *fsh);
void fs_open(FS_handle *h, const String dir);
bool isdir(String path);
String fs_parent_dir(const String path);

#endif
