#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#include "net.h"

#define TRUE   1
#define FALSE  0
#define PORT 8888

int opt = TRUE;
int main_sock;
int sd;
int max_sd;
int addrlen;
int new_socket;
int client_socket[2];
int max_clients = 2;
int activity;
int valread;
int i;
struct sockaddr_in address;
char buffer[1025];  //data buffer of 1K

int net_init() {

  //initialise all client_socket[] to 0 so not checked
  for (i = 0; i < max_clients; i++) {
    client_socket[i] = 0;
  }

  if( (main_sock = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if( setsockopt(main_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  //type of socket created
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons( PORT );

  if (bind(main_sock, (struct sockaddr *)&address, sizeof(address))<0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  if (listen(main_sock, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  addrlen = sizeof(address);
  return main_sock;
}

int net_max(fd_set *set) {

  FD_SET(main_sock, set);
  max_sd = main_sock;

  //add child sockets to set
  for ( i = 0 ; i < max_clients ; i++) {
    //socket descriptor
    sd = client_socket[i];

    //if valid socket descriptor then add to read list
    if(sd > 0)
      FD_SET( sd , set);

    //highest file descriptor number, need it for the select function
    if(sd > max_sd)
      max_sd = sd;
  }
  return max_sd;
}

void handle_new() {
  if ((new_socket = accept(main_sock, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  char *message = "ECHO Daemon v1.0 \r\n";
  //inform user of socket number - used in send and receive commands
  printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

  if( send(new_socket, message, strlen(message), 0) != (ssize_t)strlen(message) ) {
    perror("send");
  }
  //add new socket to array of sockets
  for (i = 0; i < max_clients; i++) {
    //if position is empty
    if( client_socket[i] == 0 ){
      client_socket[i] = new_socket;
      printf("Adding to list of sockets as %d %d\n" , i, new_socket);
      break;
    }
  }
}

void handle_socks(fd_set *set) {
  //else its some IO operation on some other socket :)
  for (i = 0; i < max_clients; i++) {
    sd = client_socket[i];

    if (FD_ISSET( sd , set)) {
      //Check if it was for closing , and also read the incoming message
      if ((valread = read( sd , buffer, 1024)) == 0) {
        //Somebody disconnected , get his details and print
        getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
        printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

        //Close the socket and mark as 0 in list for reuse
        close( sd );
        client_socket[i] = 0;
      }

      //Echo back the message that came in
      else {
        //set the string terminating NULL byte on the end of the data read
        buffer[valread] = '\0';
        send(sd , buffer , strlen(buffer) , 0 );
      }
    }
  }
}

void net_select(fd_set *set) {
  if (FD_ISSET(main_sock, set)) {
    handle_new();
  }
  handle_socks(set);
}
