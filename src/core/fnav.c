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
#include "net.h"

#define MAXBUFLEN 1000
struct winsize win;
pid_t child;
int fd, fd_max;
fd_set set;
int fd_in[2];
sigset_t emptyset;
int master;
int net_sock, net_fd;

int
main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv) {

  FN_cntlr *simple = cntlr_create("simple");
  FN_cntlr_func *f = malloc(sizeof(FN_cntlr_func));
  f->name = "showdat";
  f->func = showdat;

  simple->add(simple, f);
  simple->call(simple, "showdat");

  //  initscr();
  //  curs_set(0);
  //  noecho();
  net_sock = net_init();

  for(;;) {
    FD_ZERO(&set);
    FD_SET(fd, &set);
    FD_SET(STDIN_FILENO, &set);

    net_fd = net_max(&set);
    fd_max = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;
    fd_max = (fd_max > net_fd) ? fd_max : net_fd;
    pselect(fd_max+1, &set, NULL, NULL, NULL, &emptyset);
    if (FD_ISSET(STDIN_FILENO, &set)) {
      char buf;
      read(STDIN_FILENO, &buf, sizeof(buf));
      write(fd_in[1], &buf, sizeof(buf));
    }
    if (FD_ISSET(fd, &set)) {
      char buf;
      if (read(fd, &buf, sizeof(buf)) != -1) {
        write(STDOUT_FILENO, &buf, sizeof(buf));
        fflush(stdout);
      }
    }
    net_select(&set);
    //WINDOW *win = newwin(10, 10,0,0);
    //wprintw(win, "dsfsdfdsf");
    //wrefresh(win);
  }
}
