#include <stdio.h>

#include "rpc.h"
#include "fs.h"
#include "event.h"

static void fs_cb(uv_fs_t* req)
{
  uv_dirent_t dent;
  fprintf(stderr, "--fs_cb scan--\n");
  fprintf(stderr, "%s\n", req->path);
  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    Channel *channel = req->data;
    //TODO: channel->job->args[0] = dent.name;
    //      queue_push(channel->job)
    channel->job->read_cb(dent.name);
  }
}

void fs_open_cb(uv_loop_t *loop, Channel channel)
{
  fprintf(stderr, "fs_open_cb\n");
  uv_fs_t fh;
  channel.handle = (uv_handle_t*)&fh;
  fh.data = &channel;
  uv_fs_scandir(loop, &fh, channel.args[0], 0, fs_cb);
}

void fs_open(String dir, Job *job)
{
  fprintf(stderr, "fs_open req: %s\n", dir);
  String dirs[] = { dir };
  Channel channel = {
    .args = dirs,
    .job = job,
    .open_cb = fs_open_cb
    //TODO: .fn = to btree db
  };
  rpc_push_channel(channel);
}
