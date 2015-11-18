
#include <unistd.h>
#include "fnav/tui/window.h"
#include "fnav/tui/sbuffer.h"
#include "fnav/log.h"

static void on_input_cb(Buffer *buf, int key);

void sbuffer_init(Buffer *buf)
{
  log_msg("SBUFFER", "init");
  buf->input_cb = on_input_cb;
  buf_set_pass(buf);
}

void on_input_cb(Buffer *buf, int key)
{
  log_msg("SBUFFER", "on_input_cb");
}

void sbuf_write(Buffer *buf, char *str, size_t nbyte)
{
  log_msg("SBUFFER", "sbuf_write");
  //TODO: how to implement shell
  //  tigetstr from pipe
  //  strip acs name
  //  match in acs table
  //  call cb for that acs: clear, color, move, etc
  //  maintain buffer structure with colors etc per line
  if (strstr(str, "[2J")) { // test
    //window_draw_all();
  }
  else
    write(STDOUT_FILENO, str, nbyte);
}
