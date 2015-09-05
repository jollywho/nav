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
  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    Channel *channel = req->data;
    channel->job->args = malloc(sizeof(String));
    channel->job->args[0] = strdup(dent.name);
    queue_push(channel->job);
  }
}

void watch_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
  if (events & UV_RENAME)
    log_msg("FS", "=%s= renamed", filename);
  if (events & UV_CHANGE)
    log_msg("FS", "=%s= changed", filename);
}

void fs_scan_cb(Loop *loop, Channel *channel)
{
  log_msg("FS", "scan_cb");
  uv_fs_t *fh = malloc(sizeof(uv_fs_t));
  channel->handle = (uv_handle_t*)&fh;
  fh->data = channel;
  uv_fs_scandir(loop, fh, channel->args[0], 0, scan_cb);
}

void fs_watch_cb(Loop *loop, Channel *channel)
{
  log_msg("FS", "watch_cb");
  uv_fs_event_t *fh = malloc(sizeof(uv_fs_event_t));
  channel->handle = (uv_handle_t*)&fh;
  fh->data = channel;
  uv_fs_event_init(loop, fh);
  uv_fs_event_start(fh, watch_cb, channel->args[0], 0);
}

void fs_open(String dir, cntlr_cb read_cb, cntlr_cb after_cb)
{
  log_msg("FS", "open req");
  String dirs[] = { dir };
  Job *job = malloc(sizeof(Job));
  job->fn = table_add;
  job->read_cb = read_cb;
  job->after_cb = after_cb;

  Channel *scan_chan = malloc(sizeof(Channel));
  scan_chan->args = dirs;
  scan_chan->job = job;
  scan_chan->open_cb = fs_scan_cb;
  rpc_push_channel(*scan_chan);

  Channel *watch_chan = malloc(sizeof(Channel));
  watch_chan->args = dirs;
  watch_chan->job = job;
  watch_chan->open_cb = fs_watch_cb;
  rpc_push_channel(*watch_chan);
}

void stat_cb(uv_fs_t* req)
{
  log_msg("FS", "--stat--");
}

void fs_stat_cb(Loop loop, Channel channel)
{
  log_msg("FS", "stat_cb");
  uv_fs_t fh;
  channel.handle = (uv_handle_t*)&fh;
  fh.data = &channel;
  uv_fs_stat(&loop, &fh, channel.args[0], stat_cb);
}

void fs_stat(String dir, Job *job)
{
  log_msg("FS", "stat req");
  String dirs[] = { dir };
  job->fn = table_add;
  Channel channel = {
    .args = dirs,
    .job = job,
    .open_cb = fs_stat_cb
  };
  rpc_push_channel(channel);
}
