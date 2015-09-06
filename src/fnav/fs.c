#define _GNU_SOURCE
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "fnav/rpc.h"
#include "fnav/fs.h"
#include "fnav/event.h"
#include "fnav/table.h"
#include "fnav/log.h"

void scan_cb(uv_fs_t* req)
{
  log_msg("FS", "--scan--");
  log_msg("FS", "%s", req->path);
  uv_dirent_t dent;
  Channel *channel = req->data;
  FS_handle *fh = channel->handle;
  while (UV_EOF != uv_fs_scandir_next(req, &dent) && !fh->cancel) {
    channel->job->args = malloc(sizeof(String));
    channel->job->args[0] = strdup(req->path);
    channel->job->args[1] = strdup(dent.name);
    queue_push(channel->job);
  }
  uv_fs_req_cleanup((uv_fs_t*)channel->uv_handle);
  channel->close_cb(fh);
}

void watch_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
  if (events & UV_RENAME)
    log_msg("FS", "=%s= renamed", filename);
  if (events & UV_CHANGE)
    log_msg("FS", "=%s= changed", filename);
}

void fs_open_cb(Loop *loop, Channel *channel)
{
  log_msg("FS", "scan_cb");
  uv_fs_t *fs_h = malloc(sizeof(uv_fs_t));
  channel->uv_handle = (uv_handle_t*)fs_h;
  fs_h->data = channel;
  uv_fs_event_init(loop, channel->watcher);
  uv_fs_event_start(channel->watcher, watch_cb, channel->args[0], 0);
  uv_fs_scandir(loop, (uv_fs_t*)channel->uv_handle, channel->args[0], 0, scan_cb);
}

void fs_close_cb(FS_handle *h)
{
  log_msg("FM", "reset");
  h->cancel = false;
}

void fs_open(FS_handle *h, String dir)
{
  if (h->cur_dir == dir)
    return;
  h->cur_dir = dir;
  h->channel->args[0] = dir;
  rpc_push_channel(h->channel);
}

FS_handle* fs_init(Cntlr *c, String dir, cntlr_cb read_cb, cntlr_cb after_cb)
{
  log_msg("FS", "open req");
  String dirs[] = { dir };

  FS_handle *fh = malloc(sizeof(FS_handle));
  Job *job = malloc(sizeof(Job));
  Channel *channel = malloc(sizeof(Channel));
  channel->watcher = malloc(sizeof(uv_fs_event_t));

  job->caller = c;
  job->fn = table_add;
  job->read_cb = read_cb;
  job->after_cb = after_cb;

  channel->args = dirs;
  channel->open_cb = fs_open_cb;
  channel->close_cb = fs_close_cb;
  channel->job = job;
  channel->handle = fh;

  fh->channel = channel;
  return fh;
}
