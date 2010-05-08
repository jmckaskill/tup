/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int openat(int dirfd, const char *pathname, int flags, ...)
{
	int fd;
	mode_t mode = 0;

	if(pthread_mutex_lock(&dir_mutex) < 0) {
		perror("pthread_mutex_lock");
		return -1;
	}

	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_unlock;
	}
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	fd = open(pathname, flags, mode);
	pthread_mutex_unlock(&dir_mutex);
	return fd;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
