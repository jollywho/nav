#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "fnav/event/wstream.h"
#include "fnav/log.h"

#define DEFAULT_MAXMEM 1024 * 1024 * 10
static void write_cb(uv_write_t *req, int status);

typedef struct {
  Stream *stream;
  WBuffer *buffer;
  uv_write_t uv_req;
} WRequest;

void wstream_init_fd(Loop *loop, Stream *stream, int fd, size_t maxmem,
    void *data)
{
  log_msg("WSTREAM", "rstream_init_fd");
  stream_init(loop, stream, fd, NULL, data);
  wstream_init(stream, maxmem);
}

void wstream_init_stream(Stream *stream, uv_stream_t *uvstream, size_t maxmem,
    void *data)
{
  log_msg("WSTREAM", "rstream_init_stream");
  stream_init(NULL, stream, -1, uvstream, data);
  wstream_init(stream, maxmem);
}

void wstream_init(Stream *stream, size_t maxmem)
{
  log_msg("WSTREAM", "init");
  stream->maxmem = maxmem ? maxmem : DEFAULT_MAXMEM;
}

/// Sets a callback that will be called on completion of a write request,
/// indicating failure/success.
///
/// This affects all requests currently in-flight as well. Overwrites any
/// possible earlier callback.
///
/// @note This callback will not fire if the write request couldn't even be
///       queued properly (i.e.: when `wstream_write() returns an error`).
///
/// @param stream The `Stream` instance
/// @param cb The callback
void wstream_set_write_cb(Stream *stream, stream_write_cb cb)
{
  log_msg("WSTREAM", "wstream_set_write_cb");
  stream->write_cb = cb;
}

/// Queues data for writing to the backing file descriptor of a `Stream`
/// instance. This will fail if the write would cause the Stream use more
/// memory than specified by `maxmem`.
///
/// @param stream The `Stream` instance
/// @param buffer The buffer which contains data to be written
/// @return false if the write failed
bool wstream_write(Stream *stream, WBuffer *buffer)
{
  log_msg("WSTREAM", "wstream_write");
  if (stream->curmem > stream->maxmem) {
    goto err;
  }

  stream->curmem += buffer->size;

  WRequest *data = malloc(sizeof(WRequest));
  data->stream = stream;
  data->buffer = buffer;
  data->uv_req.data = data;

  uv_buf_t uvbuf;
  uvbuf.base = buffer->data;
  uvbuf.len = buffer->size;

  if (uv_write(&data->uv_req, stream->uvstream, &uvbuf, 1, write_cb)) {
    free(data);
    goto err;
  }

  stream->pending_reqs++;
  return true;

err:
  wstream_release_wbuffer(buffer);
  return false;
}

/// Creates a WBuffer object for holding output data. Instances of this
/// object can be reused across Stream instances, and the memory is freed
/// automatically when no longer needed(it tracks the number of references
/// internally)
///
/// @param data Data stored by the WBuffer
/// @param size The size of the data array
/// @param refcount The number of references for the WBuffer. This will be used
///        by Stream instances to decide when a WBuffer should be freed.
/// @param cb Pointer to function that will be responsible for freeing
///        the buffer data(passing 'free' will work as expected).
/// @return The allocated WBuffer instance
WBuffer* wstream_new_buffer(char *data, size_t size, size_t refcount,
  wbuffer_data_finalizer cb)
{
  log_msg("WSTREAM", "wstream_new_buffer");
  WBuffer *rv = malloc(sizeof(WBuffer));
  rv->size = size;
  rv->refcount = refcount;
  rv->cb = cb;
  rv->data = data;

  return rv;
}

static void write_cb(uv_write_t *req, int status)
{
  log_msg("WSTREAM", "write_cb");
  WRequest *data = req->data;

  data->stream->curmem -= data->buffer->size;

  wstream_release_wbuffer(data->buffer);

  if (data->stream->write_cb) {
    data->stream->write_cb(data->stream, data->stream->data, status);
  }

  data->stream->pending_reqs--;

  if (data->stream->closed && data->stream->pending_reqs == 0) {
    // Last pending write, free the stream;
    stream_close_handle(data->stream);
  }

  free(data);
}

void wstream_release_wbuffer(WBuffer *buffer)
{
  if (!--buffer->refcount) {
    if (buffer->cb) {
      buffer->cb(buffer->data);
    }

    free(buffer);
  }
}
