#ifndef _FNAV_H
#define _FNAV_H

#define PORT "9034"

struct winsize win;
pid_t child;
int fd, fd_max;
fd_set set;
int fd_in[2];
sigset_t emptyset;
int master;

#endif
