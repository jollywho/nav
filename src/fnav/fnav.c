#include <string.h>
#include <curses.h>
#include <locale.h>

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

  option_init();
  config_init();
  config_load_defaults();

  tables_init();
  event_init();
  input_init();
  compl_init();
  window_init();
  shell_init();
  log_msg("INIT", "INIT_END");
}

void cleanup(void)
{
  log_msg("CLEANUP", "CLEANUP_START");
  shell_cleanup();
  window_cleanup();
  compl_cleanup();
  input_cleanup();
  event_cleanup();
  tables_cleanup();
  option_cleanup();
  vt_shutdown();
  endwin();
  log_msg("CLEANUP", "CLEANUP_END");
  //logger
}

int main(int argc, char **argv)
{
  init();
  start_event_loop();
  config_write_info();
  cleanup();
  log_msg("INIT", "END_OF_EXECUTION");
}
