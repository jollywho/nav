// ex_cmd
// command line
#include "fnav/log.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/tui/sh_cntlr.h"
#include "fnav/cmdstr.h"

void ex_input(BufferNode *bn, int key)
{
  log_msg("EXCMD", "input");
  // TODO: excmd would normally read from buffer for this
  if (key == 'o') {
    fm_init(bn->buf);
    window_ex_cmd_end();
  }
  if (key == '1') {
    sh_init(bn->buf);
    window_ex_cmd_end();
  }
}

void window_ex_cmd()
{
  log_msg("EXCMD", "window_ex_cmd");
  char *test = "yqs eii {name: something age:>1995 multi:\"long enough\"}";
  tokenize_line(&test);
}
