#include <malloc.h>
#include <stddef.h>
#include <string.h>

#include "nav.h"
#include "nav/rbuffer.h"
#include "nav/log.h"

/// Creates a new `RBuffer` instance.
RBuffer* rbuffer_new(size_t capacity)
{
  log_msg("RBUFFER", "new");
  if (!capacity) {
    capacity = 0xffff;
  }

  RBuffer *rv = malloc(sizeof(RBuffer) + capacity);
  rv->full_cb = rv->nonfull_cb = NULL;
  rv->data = NULL;
  rv->size = 0;
  rv->write_ptr = rv->read_ptr = rv->start_ptr;
  rv->end_ptr = rv->start_ptr + capacity;
  return rv;
}

void rbuffer_free(RBuffer *buf)
{
  log_msg("RBUFFER", "free");
  free(buf);
}

size_t rbuffer_size(RBuffer *buf)
{
  return buf->size;
}

size_t rbuffer_capacity(RBuffer *buf)
{
  return (size_t)(buf->end_ptr - buf->start_ptr);
}

size_t rbuffer_space(RBuffer *buf)
{
  return rbuffer_capacity(buf) - buf->size;
}

/// Return a pointer to a raw buffer containing the first empty slot available
/// for writing. The second argument is a pointer to the maximum number of
/// bytes that could be written.
///
/// It is necessary to call this function twice to ensure all empty space was
/// used. See RBUFFER_UNTIL_FULL for a macro that simplifies this task.
char* rbuffer_write_ptr(RBuffer *buf, size_t *write_count)
{
  log_msg("RBUFFER", "write_ptr");
  if (buf->size == rbuffer_capacity(buf)) {
    *write_count = 0;
    return NULL;
  }

  if (buf->write_ptr >= buf->read_ptr) {
    *write_count = (size_t)(buf->end_ptr - buf->write_ptr);
  } else {
    *write_count = (size_t)(buf->read_ptr - buf->write_ptr);
  }

  log_msg("RBUFFER", "write_ptr end");
  return buf->write_ptr;
}

// Set read and write pointer for an empty RBuffer to the beginning of the
// buffer.
void rbuffer_reset(RBuffer *buf)
{
  if (buf->size == 0) {
    buf->write_ptr = buf->read_ptr = buf->start_ptr;
  }
}

/// Adjust `rbuffer` write pointer to reflect produced data. This is called
/// automatically by `rbuffer_write`, but when using `rbuffer_write_ptr`
/// directly, this needs to called after the data was copied to the internal
/// buffer. The write pointer will be wrapped if required.
void rbuffer_produced(RBuffer *buf, size_t count)
{
  buf->write_ptr += count;
  if (buf->write_ptr >= buf->end_ptr) {
    // wrap around
    buf->write_ptr -= rbuffer_capacity(buf);
  }

  buf->size += count;
  if (buf->full_cb && !rbuffer_space(buf)) {
    buf->full_cb(buf, buf->data);
  }
}

/// Return a pointer to a raw buffer containing the first byte available
/// for reading. The second argument is a pointer to the maximum number of
/// bytes that could be read.
///
/// It is necessary to call this function twice to ensure all available bytes
/// were read. See RBUFFER_UNTIL_EMPTY for a macro that simplifies this task.
char* rbuffer_read_ptr(RBuffer *buf, size_t *read_count)
{
  log_msg("RBUFFER", "read_ptr");
  if (!buf->size) {
    *read_count = 0;
    return NULL;
  }

  if (buf->read_ptr < buf->write_ptr) {
    *read_count = (size_t)(buf->write_ptr - buf->read_ptr);
  } else {
    *read_count = (size_t)(buf->end_ptr - buf->read_ptr);
  }

  return buf->read_ptr;
}

/// Adjust `rbuffer` read pointer to reflect consumed data. This is called
/// automatically by `rbuffer_read`, but when using `rbuffer_read_ptr`
/// directly, this needs to called after the data was copied from the internal
/// buffer. The read pointer will be wrapped if required.
void rbuffer_consumed(RBuffer *buf, size_t count)
{
  log_msg("RBUFFER", "consumed");
  buf->read_ptr += count;
  if (buf->read_ptr >= buf->end_ptr) {
      buf->read_ptr -= rbuffer_capacity(buf);
  }

  bool was_full = buf->size == rbuffer_capacity(buf);
  buf->size -= count;
  if (buf->nonfull_cb && was_full) {
    buf->nonfull_cb(buf, buf->data);
  }
}

// Higher level functions for copying from/to RBuffer instances and data
// pointers
size_t rbuffer_write(RBuffer *buf, char *src, size_t src_size)
{
  log_msg("RBUFFER", "write");
  size_t size = src_size;

  RBUFFER_UNTIL_FULL(buf, wptr, wcnt) {
    size_t copy_count = MIN(src_size, wcnt);
    memcpy(wptr, src, copy_count);
    rbuffer_produced(buf, copy_count);

    if (!(src_size -= copy_count)) {
      return size;
    }

    src += copy_count;
  }

  return size - src_size;
}

size_t rbuffer_read(RBuffer *buf, char *dst, size_t dst_size)
{
  log_msg("RBUFFER", "read");
  size_t size = dst_size;

  RBUFFER_UNTIL_EMPTY(buf, rptr, rcnt) {
    size_t copy_count = MIN(dst_size, rcnt);
    memcpy(dst, rptr, copy_count);
    rbuffer_consumed(buf, copy_count);

    if (!(dst_size -= copy_count)) {
      return size;
    }

    dst += copy_count;
  }

  return size - dst_size;
}

char* rbuffer_get(RBuffer *buf, size_t index)
{
  log_msg("RBUFFER", "rbuffer_get");
  char *rptr = buf->read_ptr + index;
  if (rptr >= buf->end_ptr) {
    rptr -= rbuffer_capacity(buf);
  }
  return rptr;
}

int rbuffer_cmp(RBuffer *buf, const char *str, size_t count)
{
  size_t rcnt;
  (void)rbuffer_read_ptr(buf, &rcnt);
  size_t n = MIN(count, rcnt);
  int rv = memcmp(str, buf->read_ptr, n);
  count -= n;
  size_t remaining = buf->size - rcnt;

  if (rv || !count || !remaining) {
    return rv;
  }

  return memcmp(str + n, buf->start_ptr, count);
}
