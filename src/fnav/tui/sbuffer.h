#ifndef FN_TUI_SBUFFER_H
#define FN_TUI_SBUFFER_H

#include "fnav/tui/buffer.h"

void sbuffer_init(Buffer *buf);
void sbuffer_start(Buffer *buf);
void sbuffer_stop(Buffer *buf);

void sbuf_write(Buffer *buf, char *str, size_t nbyte);

#endif
