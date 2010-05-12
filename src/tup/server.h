/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"

#include <pthread.h>

#ifndef _WIN32
#include <signal.h>
#include <sys/un.h>
#endif

#ifdef _WIN32
typedef uintptr_t socket_t;
#else
typedef int socket_t;
#define closesocket(x) close(x)
#define INVALID_SOCKET -1
#endif

struct server {
	socket_t sd[2];
	fd_t lockfd;
	pthread_t tid;
	struct file_info finfo;
	int udp_port;
};

int server_init(void);
void server_setenv(struct server *s, int vardict_fd);
int start_server(struct server *s);
int stop_server(struct server *s);

#endif
