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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#include "fnav.h"
#include "controller.h"

#define MAXBUFLEN 1000
struct winsize win;
pid_t child;
int fd, fd_max;
fd_set set;
int fd_in[2];
sigset_t emptyset;
int master;

int
main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv) {

  FN_cntlr *simple = cntlr_create("simple");
  FN_cntlr_func *f = malloc(sizeof(FN_cntlr_func));
  f->name = "showdat";
  f->func = showdat;

  simple->add(simple, f);
  simple->call(simple, "showdat");

  initscr();
  curs_set(0);
  noecho();
  ////////////////////////////////
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  struct sockaddr_storage;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
      perror("listener: socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2;
  }

  freeaddrinfo(servinfo);
  ///////////////////////////////////
  for(;;) {
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    FD_SET(fd, &set);
    FD_SET(STDIN_FILENO, &set);

    fd_max = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;
    fd_max = (fd_max > sockfd) ? fd_max : sockfd;
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
    if (FD_ISSET(sockfd, &set)) {
      char buf[90];
      int nbytes;
      if ((nbytes = recv(sockfd, buf, sizeof buf, 0)) <= 0) {
      }
      WINDOW *win = newwin(10, 10,0,0);
      wprintw(win, "dsfsdfdsf");
      wrefresh(win);
      delwin(win);
    }
  }
}
