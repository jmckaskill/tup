/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#define _GNU_SOURCE
#include "fd.h"
#include "compat.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#ifdef _WIN32
#include <compat/win32/misc.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#endif

static void dupbuf(struct buf* from, struct buf* to)
{
	to->len = from->len;
	to->s = (char*) malloc(from->len + 1);
	memcpy(to->s, from->s, from->len + 1);
}

#ifdef _WIN32
static void appendcwd(struct buf* to)
{
	to->len = GetCurrentDirectoryA(0, NULL) - 1;
	to->s = (char*) malloc(to->len + 2);
	GetCurrentDirectoryA(to->len + 1, to->s);
	to->len++;
	to->s[to->len - 1] = path_sep;
	to->s[to->len] = '\0';
}

#else
static void appendcwd(struct buf* to)
{
	to->len = pathconf(".", _PC_PATH_MAX);
	to->s = (char*) malloc(to->len + 2);
	getcwd(to->s, to->len - 1);
	to->s[to->len] = path_sep;
	to->s[to->len + 1] = '\0';
	to->len = strlen(to->s);
}
#endif

/* If parent is NULL, we prepend the current working directory */
static void relname(struct buf* parent, const char* name, struct buf* dest)
{
	char* to;
	size_t namelen = strlen(name);

	if (is_path_abs(name)) {
		dest->len = namelen;
		dest->s = (char*) malloc(dest->len + 1);
		to = dest->s;

	} else if (parent) {
		dest->len = namelen + parent->len + 1;
		dest->s = (char*) malloc(dest->len + 1);
		memcpy(dest->s, parent->s, parent->len);
		to = dest->s + parent->len;
		*(to++) = path_sep;

	} else {
		appendcwd(dest);
		dest->s = (char*) realloc(dest->s, dest->len + namelen + 2);
		to = dest->s + dest->len;
		dest->len += namelen;

	}

	memcpy(to, name, namelen);
	to[namelen] = '\0';
}




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






















#elif defined _WIN32

char* dup_filename(fd_t dir, const char* file)
{
	size_t filesz = strlen(file);
	char* buf = (char*) malloc(dir.u.dir.len + filesz + 2);
	char* to = buf;
	memcpy(to, dir.u.dir.s, dir.u.dir.len);
	to += dir.u.dir.len;
	*(to++) = path_sep;
	memcpy(to, file, filesz);
	*to = '\0';
	return to;
}


int fd_open(const char* name, int flags, fd_t* pfd)
{
	DWORD dwCreationDisposition = (flags & O_TRUNC) ? TRUNCATE_EXISTING : OPEN_ALWAYS;
	DWORD dwDesiredAccess = flags & ~O_TRUNC;
	DWORD dwShareMode = (flags & GENERIC_WRITE) ? FILE_SHARE_WRITE : FILE_SHARE_READ;
	DWORD attributes = GetFileAttributesA(name);

	if (attributes == INVALID_FILE_ATTRIBUTES) {
		set_errno_from_winerr();
		return -1;
	} else if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
		pfd->is_dir = 1;
		relname(NULL, name, &pfd->u.dir);
		return 0;
	} else {
		pfd->is_dir = 0;
		pfd->u.file = CreateFileA(
				name,
				dwDesiredAccess,
				dwShareMode,
				NULL,
				dwCreationDisposition,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

		if (pfd->u.file == INVALID_HANDLE_VALUE) {
			set_errno_from_winerr();
			return -1;
		}

		return 0;
	}
}

int fd_create(const char* name, int flags, int mode, fd_t* pfd)
{
	(void) mode;
	pfd->is_dir = 0;
	pfd->u.file = CreateFileA(
			name,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_WRITE,
		       	NULL,
			CREATE_ALWAYS,
		       	FILE_ATTRIBUTE_NORMAL,
		       	NULL);

	if (pfd->u.file == INVALID_HANDLE_VALUE) {
		set_errno_from_winerr();
		return -1;
	}

	if (flags & O_TRUNC)
		fd_truncate(*pfd, 0);

	return 0;
}

int fd_openat(fd_t dir, const char* name, int flags, fd_t* pfd)
{
	struct buf b;
	int ret;

	relname(&dir.u.dir, name, &b);
	ret = fd_open(b.s, flags, pfd);
	free(b.s);
	return ret;
}

int fd_createat(fd_t dir, const char* name, int flags, int mode, fd_t* pfd)
{
	struct buf b;
	int ret;

	(void) mode;

	relname(&dir.u.dir, name, &b);
	ret = fd_create(b.s, flags, mode, pfd);
	free(b.s);
	return ret;
}

int fd_dup(fd_t file, fd_t* pfd)
{
	pfd->is_dir = file.is_dir;
	if (file.is_dir) {
		dupbuf(&file.u.dir, &pfd->u.dir);
		return 0;
	} else {
		return DuplicateHandle(
				GetCurrentProcess(),
			       	file.u.file,
			       	GetCurrentProcess(),
				&pfd->u.file,
				0,
				TRUE,
				DUPLICATE_SAME_ACCESS);

	}
}

void fd_close(fd_t file)
{
	if (file.is_dir) {
		free(file.u.dir.s);
	} else {
		CloseHandle(file.u.file);
	}
}

int fd_chdir(fd_t dir)
{ return SetCurrentDirectoryA(dir.u.dir.s) ? 0 : -1; }

int fd_mkdirat(fd_t dir, const char* name, int mode)
{
	struct buf b;
	BOOL ret;

	(void) mode;

	relname(&dir.u.dir, name, &b);
	ret = CreateDirectoryA(b.s, NULL);
	free(b.s);

	if (!ret) {
		set_errno_from_winerr();
		return -1;
	}

	return 0;
}

int fd_readlinkat(fd_t dir, const char* name, char* buf, size_t bufsz)
{ 
	(void) dir;
	(void) name;
	(void) buf;
	(void) bufsz;
	return -1; 
}

int fd_unlinkat(fd_t dir, const char* name)
{
	struct buf b;
	BOOL ret;

	relname(&dir.u.dir, name, &b);
	ret = DeleteFileA(b.s);
	free(b.s);

	if (!ret) {
		set_errno_from_winerr();
		return -1;
	}

	return 0;
}

int fd_lstatat(fd_t dir, const char* name, struct stat* buf)
{
	struct buf b;
	int ret;

	relname(&dir.u.dir, name, &b);
	ret = lstat(b.s, buf);
	free(b.s);
	return ret;
}

int fd_fstat(fd_t f, struct stat* buf)
{
	if (f.is_dir) {
		return stat(f.u.dir.s, buf);
	} else {
		BY_HANDLE_FILE_INFORMATION info;
		struct timeval tv;

		if (!GetFileInformationByHandle(f.u.file, &info))
			return -1;

		filetime_to_timeval(&info.ftLastWriteTime, &tv);

		buf->st_mode = info.dwFileAttributes;
		buf->st_mtime = tv.tv_sec;
		buf->st_size = ((uint64_t) info.nFileSizeHigh) << 32 
			| (uint64_t) info.nFileSizeLow;

		return 0;
	}
}

int fd_pipe(fd_t socks[2])
{
	HANDLE read, write;
	if (!CreatePipe(&read, &write, NULL, 0))
		return -1;

	socks[0].is_dir = 0;
	socks[0].u.file = read;
	socks[1].is_dir = 0;
	socks[1].u.file = write;
	return 0;
}

int fd_write(fd_t file, const void *buf, size_t sz)
{
	DWORD written;
	if (!WriteFile(file.u.file, buf, sz, &written, NULL))
		return -1;

	return written;
}

int fd_read(fd_t file, void *buf, size_t sz)
{
	DWORD read;
	if (!ReadFile(file.u.file, buf, sz, &read, NULL))
		return -1;

	return read;
}

int fd_send(fd_t sock, const void *buf, size_t sz, int flags)
{ 
	(void) flags;
	return fd_write(sock, buf, sz); 
}

int fd_recv(fd_t sock, void *buf, size_t sz, int flags)
{ 
	(void) flags;
	return fd_read(sock, buf, sz); 
}

int fd_truncate(fd_t file, size_t sz)
{
	if (SetFilePointer(file.u.file, sz, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		return -1;

	return SetEndOfFile(file.u.file) ? 0 : -1;
}

int fd_lock(fd_t file)
{ return LockFile(file.u.file, 0, 0, 0, 0) ? 0 : -1; }

int fd_unlock(fd_t file)
{ return UnlockFile(file.u.file, 0, 0, 0, 0) ? 0 : -1; }

int fd_wait_lock(fd_t file)
{ return LockFileEx(file.u.file, LOCKFILE_EXCLUSIVE_LOCK, 0, 0, 0, NULL) ? 0 : -1; }


#else


int fd_open(const char* name, int flags, fd_t* pfd)
{
	pfd->fd = open(name, flags);
	if (pfd->fd < 0)
		return -1;

	relname(NULL, name, &pfd->name);
	return 0;
}

int fd_create(const char* name, int flags, int mode, fd_t* pfd)
{
	pfd->fd = open(name, flags | O_CREAT, mode);
	if (pfd->fd < 0)
		return -1;

	relname(NULL, name, &pfd->name);
	return 0;
}

int fd_openat(fd_t dir, const char* name, int flags, fd_t* pfd)
{
	relname(&dir.name, name, &pfd->name);
	pfd->fd = open(pfd->name.s, flags);
	if (pfd->fd < 0) {
		free(pfd->name.s);
		return -1;
	}

	return 0;
}

int fd_createat(fd_t dir, const char* name, int flags, int mode, fd_t* pfd)
{
	relname(&dir.name, name, &pfd->name);
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

	relname(&dir.name, name, &tmpname);
	rc = mkdir(tmpname.s, mode);
	free(tmpname.s);
	return rc;
}

int fd_readlinkat(fd_t dir, const char* name, char* buf, size_t bufsz)
{
	int rc;
	struct buf tmpname;

	relname(&dir.name, name, &tmpname);
	rc = readlink(tmpname.s, buf, bufsz);
	free(tmpname.s);
	return rc;
}

int fd_unlinkat(fd_t dir, const char* name)
{
	int rc;
	struct buf tmpname;

	relname(&dir.name, name, &tmpname);
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

	tmp = malloc((size_t) st.st_size + 1);
	if(!tmp) {
		return -1;
	}

	rc = fd_read(fd, tmp, (size_t) st.st_size);
	if(rc < 0) {
		goto err_out;
	}
	if(rc != (int) st.st_size) {
		goto err_out;
	}

	b->s = tmp;
	b->len = (size_t) st.st_size;
	b->s[b->len] = 0;
	return 0;

err_out:
	free(tmp);
	return -1;
}

