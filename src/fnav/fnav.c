#define _GNU_SOURCE
#include <ncurses.h>

#include "fnav/log.h"
#include "fnav/fnav.h"
#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/fm_cntlr.h"
#include "fnav/fs.h"

void init(void)
{
//  log_set("FS");
  printf("init\n");
#ifdef NCURSES_ENABLED
    initscr();
#endif
  event_init();
  queue_init();
  rpc_temp_init();
}

void sig_handler(int sig)
{
  printf("Signal received: %d\n", sig);
  endwin();
  exit(0);
}

int main(int argc, char **argv)
{
  signal(SIGINT, sig_handler);

  init();
  start_event_loop();
  endwin();
}
