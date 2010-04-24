/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#define _GNU_SOURCE
#include "fd.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#if defined TUP_HAVE_AT
int fd_open(const char* name, int flags, fd_t* pfd)
{
	pfd->fd = open(name, flags);
	if (pfd->fd < 0)
		return -1;
	return 0;
}

int fd_create(const char* name, int flags, int mode, fd_t* pfd)
{
	pfd->fd = open(name, flags | O_CREAT, mode);
	if (pfd->fd < 0)
		return -1;
	return 0;
}

int fd_openat(fd_t dir, const char* name, int flags, fd_t* pfd)
{
	pfd->fd = openat(dir.fd, name, flags);
	if (pfd->fd < 0)
		return -1;
	return 0;
}

int fd_createat(fd_t dir, const char* name, int flags, int mode, fd_t* pfd)
{
	pfd->fd = openat(dir.fd, name, flags | O_CREAT, mode);
	if (pfd->fd < 0)
		return -1;
	return 0;
}

int fd_dup(fd_t file, fd_t* pfd)
{
	pfd->fd = dup(file.fd);
	if (pfd->fd < 0)
		return -1;
	return 0;
}

void fd_close(fd_t file)
{ close(file.fd); }

int fd_mkdirat(fd_t dir, const char* name, int mode)
{ return mkdirat(dir.fd, name, mode); }

int fd_readlinkat(fd_t dir, const char* name, char* buf, size_t bufsz)
{ return readlinkat(dir.fd, name, buf, bufsz); }

int fd_unlinkat(fd_t dir, const char* name)
{ return unlinkat(dir.fd, name, 0); }

int fd_lstatat(fd_t dir, const char* name, struct stat* st)
{ return fstatat(dir.fd, name, st, AT_SYMLINK_NOFOLLOW); }

int fd_socketpair(fd_t socks[2], int type)
{ return socketpair(AF_UNIX, type, 0, (int*) socks); }






















#else

static void dupname(struct buf* parent, const char* name, struct buf* dest)
{
	char* to;
	size_t namelen = strlen(name);

	dest->len = namelen + (parent ? parent->len + 1 : 0);
	dest->s = (char*) malloc(dest->len + 1);

	to = dest->s;
	if (parent) {
		memcpy(to, parent->s, parent->len);
		to[parent->len] = '/';
		to += parent->len + 1;
	}
	memcpy(to, name, namelen);
	to[namelen] = '\0';
}

static void dupbuf(struct buf* from, struct buf* to)
{
	to->len = from->len;
	to->s = (char*) malloc(from->len);
	memcpy(to->s, from->s, from->len);
}

static void dupcwd(struct buf* to)
{
	to->len = pathconf(".", _PC_PATH_MAX);
	to->s = (char*) malloc(to->len + 1);
	getcwd(to->s, to->len);
	to->s[to->len] = '\0';
	to->len = strlen(to->s);
}

int fd_open(const char* name, int flags, fd_t* pfd)
{
	struct buf cwd;

	pfd->fd = open(name, flags);
	if (pfd->fd < 0)
		return -1;

	dupcwd(&cwd);
	dupname(&cwd, name, &pfd->name);
	free(cwd.s);
	return 0;
}

int fd_create(const char* name, int flags, int mode, fd_t* pfd)
{
	struct buf cwd;

	pfd->fd = open(name, flags | O_CREAT, mode);
	if (pfd->fd < 0)
		return -1;

	dupcwd(&cwd);
	dupname(NULL, name, &pfd->name);
	free(cwd.s);
	return 0;
}

int fd_openat(fd_t dir, const char* name, int flags, fd_t* pfd)
{
	dupname(&dir.name, name, &pfd->name);
	pfd->fd = open(pfd->name.s, flags);
	if (pfd->fd < 0) {
		free(pfd->name.s);
		return -1;
	}

	return 0;
}

int fd_createat(fd_t dir, const char* name, int flags, int mode, fd_t* pfd)
{
	dupname(&dir.name, name, &pfd->name);
	pfd->fd = open(pfd->name.s, flags | O_CREAT, mode);
	if (pfd->fd < 0) {
		free(pfd->name.s);
		return -1;
	}

	return 0;
}

int fd_dup(fd_t file, fd_t* pfd)
{
	pfd->fd = dup(file.fd);        
	if (pfd->fd < 0)
		return -1;

	dupbuf(&file.name, &pfd->name);
	return 0;
}

void fd_close(fd_t file)
{
	free(file.name.s);
	close(file.fd);
}

int fd_mkdirat(fd_t dir, const char* name, int mode)
{
	int rc;
	struct buf tmpname;

	dupname(&dir.name, name, &tmpname);
	rc = mkdir(tmpname.s, mode);
	free(tmpname.s);
	return rc;
}

int fd_readlinkat(fd_t dir, const char* name, char* buf, size_t bufsz)
{
	int rc;
	struct buf tmpname;

	dupname(&dir.name, name, &tmpname);
	rc = readlink(tmpname.s, buf, bufsz);
	free(tmpname.s);
	return rc;
}

int fd_unlinkat(fd_t dir, const char* name)
{
	int rc;
	struct buf tmpname;

	dupname(&dir.name, name, &tmpname);
	rc = unlink(tmpname.s);
	free(tmpname.s);
	return rc;
}

int fd_lstatat(fd_t dir, const char* name, struct stat* buf)
{
	int rc;
	struct buf tmpname;

	dupname(&dir.name, name, &tmpname);
	rc = lstat(tmpname.s, buf);
	free(tmpname.s);
	return rc;
}

int fd_socketpair(fd_t socks[2], int type)
{
	int fds[2];

	memset(socks, 0, sizeof(fd_t) * 2);
	if (socketpair(AF_UNIX, type, 0, fds))
		return -1;

	socks[0].fd = fds[0];
	socks[1].fd = fds[1];
	return 0;
}












#endif



#if !defined _WIN32
/* Common stuff between the two unix methods */

int fd_chdir(fd_t dir)
{ return fchdir(dir.fd); }

int fd_fstat(fd_t f, struct stat* st)
{ return fstat(f.fd, st); }

int fd_lstat(const char* name, struct stat* st)
{ return lstat(name, st); }

int fd_send(fd_t sock, const void* buf, size_t sz, int flags)
{
	assert(flags == 0);
	return send(sock.fd, buf, sz, flags);
}

int fd_recv(fd_t sock, void* buf, size_t sz, int flags)
{
	assert(flags == 0);
	return recv(sock.fd, buf, sz, flags);
}

int fd_read(fd_t file, void* buf, size_t sz)
{ return read(file.fd, buf, sz); }

int fd_write(fd_t file, const void* buf, size_t sz)
{ return write(file.fd, buf, sz); }

int fd_truncate(fd_t file, size_t sz)
{ return ftruncate(file.fd, sz); }

int fd_lock(fd_t fd)
{
	struct flock fl;
        fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if(fcntl(fd.fd, F_SETLKW, &fl) < 0) {
		perror("fcntl F_WRLCK");
		return -1;
	}
	return 0;
}

int fd_unlock(fd_t fd)
{
	struct flock fl;
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if(fcntl(fd.fd, F_SETLKW, &fl) < 0) {
		perror("fcntl F_UNLCK");
		return -1;
	}
	return 0;
}

int fd_wait_lock(fd_t fd)
{
	struct flock fl;

	while(1) {
		struct timespec ts = {0, 10000000};

		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;

		if(fcntl(fd.fd, F_GETLK, &fl) < 0) {
			perror("fcntl F_GETLK");
			return -1;
		}

		if(fl.l_type == F_WRLCK)
			break;
		nanosleep(&ts, NULL);
	}
	return 0;
}
#endif








int fd_slurp(fd_t fd, struct buf *b)
{
	struct stat st;
	char *tmp;
	int rc;

	if(fd_fstat(fd, &st) < 0) {
		return -1;
	}

	tmp = malloc(st.st_size + 1);
	if(!tmp) {
		return -1;
	}

	rc = fd_read(fd, tmp, st.st_size);
	if(rc < 0) {
		goto err_out;
	}
	if(rc != st.st_size) {
		goto err_out;
	}

	b->s = tmp;
	b->len = st.st_size;
	b->s[b->len] = 0;
	return 0;

err_out:
	free(tmp);
	return -1;
}

