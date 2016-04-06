#include <string.h>
#include <curses.h>
#include <locale.h>
#include <unistd.h>
#include <getopt.h>

#include "nav/log.h"
#include "nav/nav.h"
#include "nav/option.h"
#include "nav/config.h"
#include "nav/tui/window.h"
#include "nav/tui/ex_cmd.h"
#include "nav/event/event.h"
#include "nav/event/input.h"
#include "nav/tui/history.h"
#include "nav/table.h"
#include "nav/compl.h"
#include "nav/event/hook.h"
#include "nav/event/shell.h"
#include "nav/vt/vt.h"

void init(int debug, char *config_path)
{
  log_msg("INIT", "INIT_START");
  if (debug)
    log_init();

  setlocale(LC_CTYPE, "");
  char *term = getenv("TERM");

  log_msg("INIT", "initscr");
  initscr();
  start_color();
  use_default_colors();
  noecho();
  nonl();
  //raw();
  vt_init(term);

  cmd_init();
  option_init();
  tables_init();
  event_init();
  input_init();
  compl_init();
  hook_init();
  ex_cmd_init();
  window_init();

  config_init();
  hist_init();
  config_start(config_path);

  cmd_sort_cmds();
  log_msg("INIT", "INIT_END");
}

void cleanup(void)
{
  log_msg("CLEANUP", "CLEANUP_START");
  ex_cmd_cleanup();
  hist_cleanup();
  window_cleanup();
  hook_cleanup();
  compl_cleanup();
  input_cleanup();
  event_cleanup();
  tables_cleanup();
  option_cleanup();
  cmd_cleanup();
  vt_shutdown();
  endwin();
  log_msg("CLEANUP", "CLEANUP_END");
  log_cleanup();
}

void sigsegv_handler(int sig)
{
  endwin();
  signal(sig, SIG_DFL);
  kill(getpid(), sig);
}

static const char* usage =
"Usage: nav [options] [command]\n"
"\n"
"  -c, --config=<config>  Specify an alternative config file.\n"
"  -l, --log-file=<path>  Specify an alternative log file (Requires -d).\n"
"  -V, --verbose=<group>  Verbose mode for a specific log group.\n"
"  -d, --debug            Debug mode.\n"
"  -v, --version          Print version information and exit.\n"
"  -h, --help             Print this help message and exit.\n"
"\n";

static struct option long_options[] = {
  {"config", required_argument, NULL, 'c'},
  {"log-file", required_argument, NULL, 'l'},
  {"verbose", required_argument, NULL, 'V'},
  {"debug", no_argument, NULL, 'd'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
};

int main(int argc, char **argv)
{
  int debug = 0;
  char *config_path = NULL;

  for (int i = 0; i < argc; i++) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "c:l:V:dvh", long_options, &option_index);
    if (c < 0)
      continue;

    switch (c) {
      case 'h':
        fprintf(stdout, "%s", usage);
        exit(EXIT_SUCCESS);
        break;
      case 'c':
        config_path = optarg;
        break;
      case 'l':
        debug = 1;
        log_set_file(optarg);
        break;
      case 'd':
        debug = 1;
        break;
      case 'V':
        debug = 1;
        log_set_group(optarg);
        break;
      case 'v':
        fprintf(stdout, "%s\n", NAV_LONG_VERSION);
        exit(EXIT_SUCCESS);
        break;
      default:
        fprintf(stderr, "%s", usage);
        exit(EXIT_FAILURE);
    }
  }

#ifdef DEBUG
  signal(SIGSEGV, sigsegv_handler);
#endif
  init(debug, config_path);
  start_event_loop();
  DO_EVENTS_UNTIL(!mainloop_busy());
  config_write_info();
  cleanup();
}
