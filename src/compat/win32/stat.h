/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_compat_win32_stat_h
#define tup_compat_win32_stat_h

#include <windows.h>
#include <stdint.h>
#include <time.h>
#include <stdint.h>

#define S_ISREG(mode) (((mode) & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0)
#define S_ISLNK(mode) ((mode) & FILE_ATTRIBUTE_REPARSE_POINT)
#define S_ISDIR(mode) ((mode) & FILE_ATTRIBUTE_DIRECTORY)

struct stat {
	int		st_mode;
	time_t		st_mtime;
	uint64_t	st_size;
};

void	set_errno_from_winerr(void);
int	stat(const char* name, struct stat* buf);
int	lstat(const char* name, struct stat* buf);

#endif

