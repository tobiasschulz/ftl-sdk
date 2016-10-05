
#define __FTL_INTERNAL
#include "ftl.h"
#include "ftl_private.h"

#ifndef _WIN32
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

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

int ftl_set_socket_send_buf(int socket, int buffer_space) {
	int keep_alive = 1;
	return setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_space, sizeof(buffer_space));
}

