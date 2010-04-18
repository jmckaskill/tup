/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "misc.h"
#include <windows.h>
#include <stdint.h>

int chdir(const char* path)
{ return SetCurrentDirectoryA(path) ? 0 : -1; }

int mkdir(const char* path, int mode)
{ return CreateDirectoryA(path, NULL) ? 0 : -1; }

char* getcwd(char* buf, size_t size)
{
	return GetCurrentDirectoryA(size, buf)
		? buf
		: NULL;
}

pid_t getpid(void)
{ return GetCurrentProcessId(); }

void filetime_to_timeval(FILETIME* ft, struct timeval* tv)
{
	uint64_t res = 0;

	res |= ft->dwHighDateTime;
	res <<= 32;
	res |= ft->dwLowDateTime;
	res /= 10; /* convert into microseconds */

	/* convert to unix epoch */
	res -= UINT64_C(11644473600000000);

	tv->tv_sec  = (long) (res / UINT64_C(1000000));
	tv->tv_usec = (long) (res % UINT64_C(1000000));
}

int gettimeofday(struct timeval* tv, struct timezone* tz)
{
	FILETIME ft;
	(void) tz;
	GetSystemTimeAsFileTime(&ft);
	filetime_to_timeval(&ft, tv);
	return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
	Sleep((DWORD) (req->tv_sec * 1e3 + req->tv_nsec / 1e6 + 1));
	return 0;
}

int setenv(const char* name, const char* value, int overwrite)
{
	return SetEnvironmentVariableA(name, value) ? 0 : -1;
}

