
#define __FTL_INTERNAL
#include "ftl.h"

#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

void ftl_init_sockets() {
  //BSD sockets are smarter and don't need silly init
}

int ftl_close_socket(int sock) {
  return close(sock);
}

char * ftl_get_socket_error() {
  return strerror(errno);
}

int ftl_set_socket_recv_timeout(int socket, int ms_timeout){
  struct timeval tv = {0};

  while (ms_timeout >= 1000) {
    tv.tv_sec++;
    ms_timeout -= 1000;
  }
  tv.tv_usec = ms_timeout * 1000;

  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));  
}

int ftl_set_socket_send_timeout(int socket, int ms_timeout){
  struct timeval tv = {0};

  while (ms_timeout >= 1000) {
    tv.tv_sec++;
    ms_timeout -= 1000;
  }
  tv.tv_usec = ms_timeout * 1000;

  return setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
}

int ftl_set_socket_enable_keepalive(int socket){
  int keep_alive = 1;
  return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keep_alive, sizeof(keep_alive));
}

