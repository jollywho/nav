#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <uv.h>
#include <unistd.h> //sleep

#define ERROR(msg, code) do {                                                         \
  fprintf(stderr, "%s: [%s: %s]\n", msg, uv_err_name((code)), uv_strerror((code)));   \
  assert(0);                                                                          \
} while(0);

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf);
void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void write_cb(uv_write_t *req, int status);

int total_read;
int total_written;
#define FIBMAX 25

uv_loop_t *loop;
uv_work_t work;

void work_cb(uv_work_t* req) {
  uv_stream_t *t = (uv_stream_t*)req->data;

  uv_read_start(t, alloc_cb, read_cb);
}

void after_work_cb(uv_work_t* req, int status) {
  if (status == UV_ECANCELED) {
    fprintf(stderr, "Calculation of %d cancelled\n", *(int*)req->data);
    return;
  }

  if (status) ERROR("after work", status);

  fprintf(stderr, "Finished calculating %dth fibonacci number\n", *(int*) req->data);
}
void connection_cb(uv_stream_t *server, int status)
{
  if (status) ERROR("async connect", status);

  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);

  int r = uv_accept(server, (uv_stream_t*) client);
  if (r) {
    fprintf(stderr, "errror accepting connection %d", r);
    uv_close((uv_handle_t*) client, NULL);
  } else {
    uv_work_t fib_reqs[1];
    uv_tcp_t *t = client;
    fib_reqs[0].data = (void*)t;
    uv_queue_work(loop, &fib_reqs[0], work_cb, after_work_cb);
  }
}

void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
  buf->base = malloc(size);
  buf->len = size;
}

void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
  if (nread == UV_EOF) {
    uv_close((uv_handle_t*) client, NULL);

    fprintf(stderr, "closed client connection\n");
    total_read = total_written = 0;
  } else if (nread > 0) {
    fprintf(stderr, "%ld bytes read\n", nread);
    fprintf(stderr, "Recv Val: %s\n", buf->base);
    total_read += nread;

    write_req_t *wr = (write_req_t*) malloc(sizeof(write_req_t));
    wr->buf =  uv_buf_init(buf->base, nread);
    uv_write(&wr->req, client, &wr->buf, 1/*nbufs*/, write_cb);
  }
  if (nread == 0) free(buf->base);
}

void write_cb(uv_write_t *req, int status)
{
  write_req_t* wr;
  wr = (write_req_t*) req;

  int written = wr->buf.len;
  if (status) ERROR("async write", status);
  assert(wr->req.type == UV_WRITE);
  total_written += written;
  free(wr->buf.base);
  free(wr);
}

void run_command(uv_fs_event_t *handle, const char *filename, int events, int status) {
    char path[1024];
    size_t size = 1023;
    // Does not handle error if path is longer than 1023.
    uv_fs_event_getpath(handle, path, &size);
    path[size] = '\0';

    fprintf(stderr, "Change detected in %s: ", path);
    if (events & UV_RENAME)
        fprintf(stderr, "renamed");
    if (events & UV_CHANGE)
        fprintf(stderr, "changed");

    fprintf(stderr, " %s\n", filename ? filename : "");
}

int event_init(void)
{
  loop = uv_default_loop();
  uv_fs_event_t *fs_event_req = malloc(sizeof(uv_fs_event_t));
  uv_fs_event_init(loop, fs_event_req);
  uv_fs_event_start(fs_event_req, run_command, "a", UV_FS_EVENT_RECURSIVE);

  uv_tcp_t server;
  uv_tcp_init(loop, &server);

  struct sockaddr_in bind_addr;
  int r = uv_ip4_addr("0.0.0.0", 7100, &bind_addr);
  assert(!r);
  uv_tcp_bind(&server, (struct sockaddr*)&bind_addr, 0);

  r = uv_listen((uv_stream_t*) &server, 128 /*backlog*/, connection_cb);
  if (r) ERROR("listen error", r)

  fprintf(stderr, "Listening on localhost:7100\n");

  uv_run(loop, UV_RUN_DEFAULT);
  return 0;
}
