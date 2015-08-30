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
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#include "fnav.h"
#include "controller.h"
#include "event.h"

struct termios oldtio,newtio;

void init(void)
{
  printf("init\n");
  initscr();
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

int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
  signal(SIGINT, sig_handler);
  FN_cntlr *simple = cntlr_create("simple");
  FN_cntlr_func *f = malloc(sizeof(FN_cntlr_func));
  f->name = "showdat";
  f->func = showdat;

  simple->add(simple, f);
  simple->call(simple, "showdat");

  init();
  start_event_loop();
  endwin();
}
