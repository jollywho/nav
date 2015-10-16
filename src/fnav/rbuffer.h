// Ring buffer implementation. This is basically an array that wraps read/write
// pointers around the memory region. It should be more efficient than the old
// RBuffer which required memmove() calls to relocate read/write positions.
//
// The main purpose of RBuffer is simplify memory management when reading from
// uv_stream_t instances:
//
// - The event loop writes data to a RBuffer, advancing the write pointer
// - The main loop reads data, advancing the read pointer
// - If the buffer becomes full(size == capacity) the rstream is temporarily
//   stopped(automatic backpressure handling)
//
// Reference: http://en.wikipedia.org/wiki/Circular_buffer
#ifndef FNAV_RBUFFER_H
#define FNAV_RBUFFER_H

#include <stddef.h>
#include <stdint.h>

// Macros that simplify working with the read/write pointers directly by hiding
// ring buffer wrap logic. Some examples:
//
// - Pass the write pointer to a function(write_data) that incrementally
//   produces data, returning the number of bytes actually written to the
//   ring buffer:
//
//       RBUFFER_UNTIL_FULL(rbuf, ptr, cnt)
//         rbuffer_produced(rbuf, write_data(state, ptr, cnt));
//
// - Pass the read pointer to a function(read_data) that incrementally
//   consumes data, returning the number of bytes actually read from the
//   ring buffer:
//
//       RBUFFER_UNTIL_EMPTY(rbuf, ptr, cnt)
//         rbuffer_consumed(rbuf, read_data(state, ptr, cnt));
//
// Note that the rbuffer_{produced,consumed} calls are necessary or these macros
// create infinite loops
#define RBUFFER_UNTIL_EMPTY(buf, rptr, rcnt)                                 \
  for (size_t rcnt = 0, _r = 1; _r; _r = 0)                                  \
    for (char *rptr = rbuffer_read_ptr(buf, &rcnt);                          \
         buf->size;                                                          \
         rptr = rbuffer_read_ptr(buf, &rcnt))

#define RBUFFER_UNTIL_FULL(buf, wptr, wcnt)                                  \
  for (size_t wcnt = 0, _r = 1; _r; _r = 0)                                  \
    for (char *wptr = rbuffer_write_ptr(buf, &wcnt);                         \
         rbuffer_space(buf);                                                 \
         wptr = rbuffer_write_ptr(buf, &wcnt))


// Iteration
#define RBUFFER_EACH(buf, c, i)                                              \
  for (size_t i = 0; i < buf->size; i = buf->size)                           \
      for (char c = 0;                                                       \
           i < buf->size ? ((int)(c = *rbuffer_get(buf, i))) || 1 : 0;       \
           i++)

#define RBUFFER_EACH_REVERSE(buf, c, i)                                      \
  for (size_t i = buf->size; i != SIZE_MAX; i = SIZE_MAX)                    \
      for (char c = 0;                                                       \
           i-- > 0 ? ((int)(c = *rbuffer_get(buf, i))) || 1 : 0;             \
           )

typedef struct rbuffer RBuffer;
/// Type of function invoked during certain events:
///   - When the RBuffer switches to the full state
///   - When the RBuffer switches to the non-full state
typedef void(*rbuffer_callback)(RBuffer *buf, void *data);

struct rbuffer {
  rbuffer_callback full_cb, nonfull_cb;
  void *data;
  size_t size;
  char *end_ptr, *read_ptr, *write_ptr;
  char start_ptr[];
};

void rbuffer_free(RBuffer *buf);
RBuffer *rbuffer_new(size_t capacity);
void rbuffer_free(RBuffer *buf);
size_t rbuffer_size(RBuffer *buf);
size_t rbuffer_capacity(RBuffer *buf);
size_t rbuffer_space(RBuffer *buf);
char *rbuffer_write_ptr(RBuffer *buf, size_t *write_count);
void rbuffer_reset(RBuffer *buf);
void rbuffer_produced(RBuffer *buf, size_t count);
char *rbuffer_read_ptr(RBuffer *buf, size_t *read_count);
void rbuffer_consumed(RBuffer *buf, size_t count);
size_t rbuffer_write(RBuffer *buf, char *src, size_t src_size);
size_t rbuffer_read(RBuffer *buf, char *dst, size_t dst_size);
char *rbuffer_get(RBuffer *buf, size_t index);
int rbuffer_cmp(RBuffer *buf, const char *str, size_t count);

#endif
