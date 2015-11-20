#ifndef NVIM_EVENT_WSTREAM_H
#define NVIM_EVENT_WSTREAM_H

#include "fnav/event/stream.h"

typedef struct wbuffer WBuffer;
typedef void (*wbuffer_data_finalizer)(void *data);

struct wbuffer {
  size_t size, refcount;
  char *data;
  wbuffer_data_finalizer cb;
};

void wstream_init_fd(Loop *loop, Stream *stream, int fd, size_t maxmem,
    void *data);
void wstream_init_stream(Stream *stream, uv_stream_t *uvstream, size_t maxmem,
    void *data);
void wstream_init(Stream *stream, size_t maxmem);
void wstream_set_write_cb(Stream *stream, stream_write_cb cb);
bool wstream_write(Stream *stream, WBuffer *buffer);
WBuffer *wstream_new_buffer(char *data, size_t size, size_t refcount,
  wbuffer_data_finalizer cb);
void wstream_release_wbuffer(WBuffer *buffer);

#endif
