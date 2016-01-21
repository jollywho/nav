#include <string.h>
#include <curses.h>
#include <locale.h>

#include "fnav/log.h"
#include "fnav/fnav.h"
#include "fnav/option.h"
#include "fnav/config.h"
#include "fnav/tui/window.h"
#include "fnav/tui/ex_cmd.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/event/input.h"
#include "fnav/table.h"
#include "fnav/compl.h"

void init(void)
{
//  log_set("IMG");
  log_init();
  log_msg("INIT", "__________INIT_START____________");
  setlocale(LC_CTYPE, "");

  log_msg("INIT", "initscr");
  initscr();
  start_color();
  use_default_colors();

  option_init();
  config_init();
  config_load(NULL);
  info_load(NULL);
  curs_set(0);

  tables_init();
  event_init();
  input_init();
  compl_init();
  window_init();
  log_msg("INIT", "__________INIT_END______________");
}

void cleanup(void)
{
  log_msg("CLEANUP", "_______CLEANUP_START____________");
  window_cleanup();
  compl_cleanup();
  input_cleanup();
  event_cleanup();
  tables_cleanup();
  option_cleanup();
  endwin();
  //logger
}

void sig_handler(int sig)
{
  printf("Signal received: ***%d(%s)***\n", sig, strsignal(sig));
  endwin();
  exit(0);
}

int main(int argc, char **argv)
{
  signal(SIGINT, sig_handler);
  signal(SIGSEGV, sig_handler);

  init();
  start_event_loop();
  cleanup();
  log_msg("INIT", "_______END_OF_EXECUTION_________");
}
