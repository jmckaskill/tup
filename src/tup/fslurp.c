/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "fslurp.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static int do_slurp(int fd, struct buf *b, int extra);

int fslurp(int fd, struct buf *b)
{
	return do_slurp(fd, b, 0);
}

int fslurp_null(int fd, struct buf *b)
{
	int rc;

	rc = do_slurp(fd, b, 1);
	if(rc == 0)
		b->s[b->len] = 0;
	return rc;
}

static int do_slurp(int fd, struct buf *b, int extra)
{
	struct stat st;
	char *tmp;
	int rc;

	if(fstat(fd, &st) < 0) {
		return -1;
	}

	tmp = malloc(st.st_size + extra);
	if(!tmp) {
		return -1;
	}

	rc = read(fd, tmp, st.st_size);
	if(rc < 0) {
		goto err_out;
	}
	if(rc != st.st_size) {
		errno = EIO;
		goto err_out;
	}

	b->s = tmp;
	b->len = st.st_size;
	return 0;

err_out:
	free(tmp);
	return -1;
}
