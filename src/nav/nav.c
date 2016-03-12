#include <string.h>
#include <curses.h>
#include <locale.h>
#include <unistd.h>

#include "nav/log.h"
#include "nav/nav.h"
#include "nav/option.h"
#include "nav/config.h"
#include "nav/tui/window.h"
#include "nav/tui/ex_cmd.h"
#include "nav/event/event.h"
#include "nav/event/input.h"
#include "nav/table.h"
#include "nav/compl.h"
#include "nav/event/hook.h"
#include "nav/event/shell.h"
#include "nav/vt/vt.h"

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

  ex_cmd_init();
  cmd_sort_cmds();
  log_msg("INIT", "INIT_END");
}

void cleanup(void)
{
  log_msg("CLEANUP", "CLEANUP_START");
  ex_cmd_cleanup();
  window_cleanup();
  compl_cleanup();
  input_cleanup();
  event_cleanup();
  //tables_cleanup();
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
  DO_EVENTS_UNTIL(!mainloop_busy());
  config_write_info();
  cleanup();
  log_msg("INIT", "END_OF_EXECUTION");
}
