/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifdef _WIN32
#include <winsock2.h>
#endif

#include "server.h"
#include "file.h"
#include "debug.h"
#include "getexecwd.h"
#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <compat/win32/misc.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#endif

static void *message_thread(void *arg);

static char ldpreload_path[PATH_MAX];

int server_init(void)
{
	if(snprintf(ldpreload_path, sizeof(ldpreload_path),
		    "%s/tup-ldpreload.so",
		    getexecwd()) >= (signed)sizeof(ldpreload_path)) {
		fprintf(stderr, "Error: path for tup-ldpreload.so library is "
			"too long.\n");
		return -1;
	}
	return 0;
}

void server_setenv(struct server *s, int vardict_fd)
{
#ifndef _WIN32
	char fd_name[32];
	snprintf(fd_name, sizeof(fd_name), "%i", s->sd[1]);
	fd_name[31] = 0;
	setenv(TUP_SERVER_NAME, fd_name, 1);
	snprintf(fd_name, sizeof(fd_name), "%i", vardict_fd);
	fd_name[31] = 0;
	setenv(TUP_VARDICT_NAME, fd_name, 1);
	snprintf(fd_name, sizeof(fd_name), "%i", s->lockfd);
	fd_name[31] = 0;
	setenv(TUP_LOCK_NAME, fd_name, 1);
#ifdef __APPLE__
	setenv("DYLD_FORCE_FLAT_NAMESPACE", "", 1);
	setenv("DYLD_INSERT_LIBRARIES", ldpreload_path, 1);
#else
	setenv("LD_PRELOAD", ldpreload_path, 1);
#endif

#else
	(void) s;
	(void) vardict_fd;
#endif
}

static int connect_udp(socket_t socks[2])
{
	int port;
	int sasz;
	struct sockaddr_in sa;
	socks[0] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socks[1] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (socks[0] == INVALID_SOCKET || socks[1] == INVALID_SOCKET) {
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa.sin_port = 0;

	if (bind(socks[0], (struct sockaddr*) &sa, sizeof(struct sockaddr_in))) {
		goto err;
	}

	sasz = sizeof(struct sockaddr_in);
	if (getsockname(socks[0], (struct sockaddr*) &sa, &sasz) || sasz != sizeof(struct sockaddr_in)) {
		goto err;
	}

	port = ntohs(sa.sin_port);

	if (connect(socks[1], (struct sockaddr*) &sa, sizeof(struct sockaddr_in))) {
		goto err;
	}

	return port;

err:
	closesocket(socks[1]);
	closesocket(socks[0]);
	return -1;
}

int start_server(struct server *s)
{
#ifdef _WIN32
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);
#endif
	s->udp_port = connect_udp(s->sd);
	if (s->udp_port < 0) {
		fprintf(stderr, "Failed to open UDP port\n");
		return -1;
	}

	init_file_info(&s->finfo);

	if(pthread_create(&s->tid, NULL, &message_thread, s) < 0) {
		perror("pthread_create");
		closesocket(s->sd[0]);
		closesocket(s->sd[1]);
		return -1;
	}

	return 0;
}

int stop_server(struct server *s)
{
	void *retval = NULL;
	struct access_event e;
	int rc;

	memset(&e, 0, sizeof(e));
	e.at = ACCESS_STOP_SERVER;

	rc = send(s->sd[1], (const char*) &e, sizeof(e), 0);
	if(rc != sizeof(e)) {
		perror("send");
		return -1;
	}
	pthread_join(s->tid, &retval);
	closesocket(s->sd[0]);
	closesocket(s->sd[1]);
	s->sd[0] = INVALID_SOCKET;
	s->sd[1] = INVALID_SOCKET;

#ifdef _WIN32
	WSACleanup();
#endif

	if(retval == NULL)
		return 0;
	return -1;
}

static void *message_thread(void *arg)
{
	int recvd;
	char buf[ACCESS_EVENT_MAX_SIZE];
	struct server *s = arg;

	for (;;) {
		struct access_event* event = (struct access_event*) buf;
		char *event1, *event2;

		recvd = recv(s->sd[0], buf, sizeof(buf), 0);
		if (recvd < (int) sizeof(struct access_event)) {
			perror("recv");
			break;
		}

		if(event->at == ACCESS_STOP_SERVER)
			break;

		if(event->at > ACCESS_STOP_SERVER) {
			fprintf(stderr, "Error: Received unknown access_type %d\n", event->at);
			return (void*)-1;
		}

		if(!event->len)
			continue;

		if(event->len >= PATH_MAX - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event->len);
			return (void*)-1;
		}
		if(event->len2 >= PATH_MAX - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event->len2);
			return (void*)-1;
		}

		event1 = (char*) event + sizeof(struct access_event);
		event2 = event1 + event->len + 1;

		if (recvd != (int) sizeof(struct access_event) + event->len + event->len2 + 2) {
			fprintf(stderr, "Error: Received weird size in access_event\n");
			return (void*)-1;
		}

		if (event1[event->len] != '\0' || event2[event->len2] != '\0') {
			fprintf(stderr, "Error: Missing null terminator in access_event\n");
			return (void*)-1;
		}

		if(handle_file(event->at, event1, event2, &s->finfo) < 0) {
			fprintf(stderr, "message_thread end\n");
			return (void*)-1;
		}
	}
	return NULL;
}

