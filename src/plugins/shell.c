#include "src/core/fnav.h"

void open_fifo(char** argv) {
  fd = open("fifo", O_RDWR);

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
}
