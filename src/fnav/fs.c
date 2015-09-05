#define _GNU_SOURCE
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "rpc.h"
#include "fs.h"
#include "event.h"
#include "table.h"

static void fs_cb(uv_fs_t* req)
{
  uv_dirent_t dent;
  fprintf(stderr, "--fs_cb scan--\n");
  fprintf(stderr, "%s\n", req->path);
  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    Channel *channel = req->data;
    channel->job->args = malloc(sizeof(String));
    channel->job->args[0] = strdup(dent.name);
    queue_push(*channel->job);
  }
}

void fs_open_cb(Loop *loop, Channel channel)
{
  fprintf(stderr, "fs_open_cb\n");
  uv_fs_t fh;
  channel.handle = (uv_handle_t*)&fh;
  fh.data = &channel;
  uv_fs_scandir(loop, &fh, channel.args[0], 0, fs_cb);
}

void fs_stat_cb(Loop *loop, Channel channel)
{
}

void fs_open(String dir, Job *job)
{
  fprintf(stderr, "fs_open req: %s\n", dir);
  String dirs[] = { dir };
  job->fn = table_add;
  Channel channel = {
    .args = dirs,
    .job = job,
    .open_cb = fs_open_cb
  };
  rpc_push_channel(channel);
}
