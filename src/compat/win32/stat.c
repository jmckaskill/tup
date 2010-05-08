/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "stat.h"
#include "misc.h"
#include "errno.h"

void set_errno_from_winerr(void)
{
	DWORD err = GetLastError();
	if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
		errno = ENOENT;
	} else if (err == ERROR_ALREADY_EXISTS) {
		errno = EEXIST;
	} else {
		/* Just use something else for other errors */
		errno = EACCES;
	}
}

int stat(const char *name, struct stat *buf)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	struct timeval tv;

	if (!GetFileAttributesExA(name, GetFileExInfoStandard, &data)) {
		set_errno_from_winerr();
		return -1;
	}

	filetime_to_timeval(&data.ftLastWriteTime, &tv);

	buf->st_mode  = data.dwFileAttributes;
	buf->st_mtime = tv.tv_sec;
	buf->st_size  = ((uint64_t) data.nFileSizeHigh) << 32
		| (uint64_t) data.nFileSizeLow;

	return 0;
}

int lstat(const char *name, struct stat *buf)
{
	/* TODO handle reparse points */
	return stat(name, buf);
}
