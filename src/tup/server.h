/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include <signal.h>
#include <sys/un.h>

struct server {
	fd_t sd[2];
	fd_t lockfd;
	pthread_t tid;
	struct file_info finfo;
	char file1[PATH_MAX];
	char file2[PATH_MAX];
};

int server_init(void);
void server_setenv(struct server *s, fd_t vardict_fd);
int start_server(struct server *s);
int stop_server(struct server *s);

#endif
