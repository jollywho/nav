// ex_cmd
// command line
#include "fnav/log.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/tui/window.h"
#include "fnav/tui/fm_cntlr.h"

void ex_input(BufferNode *bn, int key)
{
  log_msg("EXCMD", "input");
  // TODO: excmd would normally read from buffer for this
  fm_cntlr_init(bn->buf);
  window_ex_cmd_end();
}
