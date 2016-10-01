
#define __FTL_INTERNAL
#include "ftl.h"

#ifndef _WIN32
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <unistd.h>

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
  struct timeval tv;
  tv.tv_sec  = ms_timeout / 1000;
  tv.tv_usec = (ms_timeout % 1000) * 1000;
  FTL_LOG(FTL_LOG_INFO, "ftl_set_socket_recv_timeout: ms_timeout = %d, tv.tv_sec = %d, tv.tv_usec = %d", ms_timeout, tv.tv_sec, tv.tv_usec);
	return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));  
}

int ftl_set_socket_send_timeout(int socket, int ms_timeout){
  struct timeval tv;
  tv.tv_sec  = ms_timeout / 1000;
  tv.tv_usec = (ms_timeout % 1000) * 1000;
  FTL_LOG(FTL_LOG_INFO, "ftl_set_socket_recv_timeout: ms_timeout = %d, tv.tv_sec = %d, tv.tv_usec = %d", ms_timeout, tv.tv_sec, tv.tv_usec);
	return setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval));
}

int ftl_set_socket_enable_keepalive(SOCKET socket){
  int keep_alive = 1;
  return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keep_alive, sizeof(keep_alive));
}

int ftl_set_socket_send_buf(SOCKET socket, int buffer_space) {
	int keep_alive = 1;
	return setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_space, sizeof(buffer_space));
}

