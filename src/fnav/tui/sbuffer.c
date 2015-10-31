
#include <unistd.h>
#include "fnav/tui/window.h"
#include "fnav/tui/sbuffer.h"
#include "fnav/log.h"

static void on_input_cb(BufferNode *buf, int key);

void sbuffer_init(Buffer *buf)
{
  log_msg("SBUFFER", "init");
  buf->input_cb = on_input_cb;
  buf_set_pass(buf);
}

void on_input_cb(BufferNode *buf, int key)
{
  log_msg("SBUFFER", "on_input_cb");
}

void sbuf_write(Buffer *buf, char *str, size_t nbyte)
{
  log_msg("SBUFFER", "sbuf_write");
  write(STDOUT_FILENO, str, nbyte);
  log_msg("___", "%s", tigetstr("clear"));
  if (strstr(str, "[2J")) { // test
    refresh();
    doupdate();
  }
}
