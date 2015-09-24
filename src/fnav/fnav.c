#define _GNU_SOURCE
#include <string.h>
#include <curses.h>
#include <locale.h>

#include "fnav/log.h"
#include "fnav/fnav.h"
#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/fm_cntlr.h"
#include "fnav/fs.h"

void init(void)
{
//  log_set("BUFFER");
  log_msg("INIT", "__________INIT______________");
#ifdef NCURSES_ENABLED
  setlocale(LC_CTYPE, "");
  log_msg("INIT", "initscr");
  initscr();
  start_color();
  use_default_colors();
  init_pair(1, 5, -1);
  init_pair(2, 6, -1);
#endif
  event_init();
  queue_init();
  rpc_temp_init();
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
  endwin();
}
