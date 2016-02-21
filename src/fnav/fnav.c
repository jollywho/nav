#include <string.h>
#include <curses.h>
#include <locale.h>
#include <unistd.h>

#include "fnav/log.h"
#include "fnav/fnav.h"
#include "fnav/option.h"
#include "fnav/config.h"
#include "fnav/tui/window.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/event/event.h"
#include "fnav/event/input.h"
#include "fnav/table.h"
#include "fnav/compl.h"
#include "fnav/event/hook.h"
#include "fnav/event/shell.h"
#include "fnav/vt/vt.h"

void init(void)
{
//  log_set("IMG");
  log_init();
  log_msg("INIT", "INIT_START");
  setlocale(LC_CTYPE, "");

  log_msg("INIT", "initscr");

  char *term = getenv("TERM");

  initscr();
  start_color();
  use_default_colors();
  noecho();
  nonl();
  raw();
  vt_init(term);

  cmd_init();
  option_init();
  tables_init();
  event_init();
  input_init();
  compl_init();
  hook_init();
  window_init();

  config_init();
  config_load_defaults();
  log_msg("INIT", "INIT_END");
}

void cleanup(void)
{
  log_msg("CLEANUP", "CLEANUP_START");
  window_cleanup();
  compl_cleanup();
  input_cleanup();
  event_cleanup();
  tables_cleanup();
  option_cleanup();
  cmd_cleanup();
  vt_shutdown();
  endwin();
  log_msg("CLEANUP", "CLEANUP_END");
  //logger
}

void sig_handler(int sig)
{
  endwin();
  signal(sig, SIG_DFL);
  kill(getpid(), sig);
}

int main(int argc, char **argv)
{
#ifdef DEBUG
  signal(SIGSEGV, sig_handler);
#endif
  init();
  start_event_loop();
  config_write_info();
  cleanup();
  log_msg("INIT", "END_OF_EXECUTION");
}
