/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_fd_h
#define tup_fd_h

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#if 0
#define TUP_HAVE_AT
#endif

struct buf {
	char *s;
	int len;
};

typedef struct fdwrap fd_t;

#if defined _WIN32
struct fdwrap {
	union {
		HANDLE      file;
		struct buf  dir;
	} u;
	int is_dir : 1;
};

struct stat {
	int	st_mode;
	time_t	st_mtime;
	size_t	st_size;
};

#elif defined TUP_HAVE_AT
struct fdwrap {
	int fd;
};

#define FD_INITIALIZER {-1}


#else
struct fdwrap {
	int         fd;
	struct buf  name;
};
#define FD_INITIALIZER {-1, {NULL, 0}}

#endif

int     fd_open(const char* name, int flags, fd_t* pfd);
int     fd_create(const char* name, int flags, int mode, fd_t* pfd);
int     fd_openat(fd_t dir, const char* name, int flags, fd_t* pfd);
int     fd_createat(fd_t dir, const char* name, int flags, int mode, fd_t* pfd);
int     fd_dup(fd_t file, fd_t* pfd);
void    fd_close(fd_t file);

int     fd_chdir(fd_t dir);
int     fd_mkdirat(fd_t dir, const char* name, int mode);
int     fd_readlinkat(fd_t dir, const char* name, char* buf, size_t bufsz);
int     fd_unlinkat(fd_t dir, const char* name);
int     fd_lstatat(fd_t dir, const char* name, struct stat* buf);
int	fd_fstat(fd_t f, struct stat* buf);
int	fd_lstat(const char* name, struct stat* buf);

int     fd_socketpair(fd_t socks[2], int type);

/* Usable on sockets */
int     fd_send(fd_t sock, const void* buf, size_t sz, int flags);
int     fd_recv(fd_t sock, void* buf, size_t sz, int flags);

/* Usable on files */
int	fd_read(fd_t file, void* buf, size_t sz);
int     fd_write(fd_t file, const void* buf, size_t sz);
int     fd_truncate(fd_t file, size_t sz);
int     fd_slurp(fd_t file, struct buf *b);

/* Usable on files */
int     fd_lock(fd_t file);
int     fd_unlock(fd_t file);
int     fd_wait_lock(fd_t file);

#endif

