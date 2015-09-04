/*
 ** fnav.c
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <curses.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdlib.h>

#include "fnav.h"
#include "loop.h"
#include "event.h"
#include "fm_cntlr.h"
#include "fs.h"

void init(void)
{
  printf("init\n");
//  initscr();
  queue_init();
  fs_init();
  fm_cntlr_init();
  event_init();
  //input_init
  //tui_init
  //rpc_init
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
