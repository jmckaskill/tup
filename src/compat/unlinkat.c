/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int unlinkat(int dirfd, const char *pathname, int flags)
{
	int rc;

	if(flags != 0) {
		fprintf(stderr, "tup compat unlinkat error: flags=%i not supported\n", flags);
		return -1;
	}

	if(pthread_mutex_lock(&dir_mutex) < 0) {
		perror("pthread_mutex_lock");
		return -1;
	}

	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_unlock;
	}
	rc = unlink(pathname);
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
