#ifndef _NET_H
#define _NET_H

int net_init();
int net_max(fd_set *set);
void net_select(fd_set *set);


#endif
