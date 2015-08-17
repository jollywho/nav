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

void slave_init(void)
{
  event_init();
  //input_init
  //tui_init
  //rpc_init
  //channel_init
}

void master_init(void)
{
  //handler_init
  //worker_init
  //ctlr_init
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
  FN_cntlr *simple = cntlr_create("simple");
  FN_cntlr_func *f = malloc(sizeof(FN_cntlr_func));
  f->name = "showdat";
  f->func = showdat;

  simple->add(simple, f);
  simple->call(simple, "showdat");

  //  initscr();
  //  curs_set(0);
  //  noecho();
  slave_init();
}
