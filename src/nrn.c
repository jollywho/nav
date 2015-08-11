#define _GNU_SOURCE
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>
#include <curses.h>

struct winsize win;
pid_t child;
int fd, fd_max;
fd_set set;
int fd_in[2];
sigset_t emptyset;

int
main(__attribute__((unused)) int argc, char **argv) {

  int master;
  initscr();
  fd = open("fifo", O_RDWR);
  curs_set(0);
  noecho();
  if (pipe(fd_in) == -1) {
    perror("pipe failed");
    exit(EX_USAGE);
  }

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
  child = forkpty(&master, NULL, NULL, &win);
  if (child == -1) {
    perror("forkpty failed");
    exit(EX_OSERR);
  }

  if (child == 0) {
    dup2(fd, STDOUT_FILENO);
    dup2(fd_in[0], STDIN_FILENO);
    close(fd);
    close(fd_in[0]);
    close(fd_in[1]);
    execvp(argv[1], argv + 1);
    perror("exec failed");
  }

  for(;;) {
    FD_ZERO(&set);
    FD_SET(fd, &set);
    FD_SET(STDIN_FILENO, &set);

    fd_max = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;
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
  }
}
