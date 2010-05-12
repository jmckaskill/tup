/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_compat_win32_misc_h
#define tup_compat_win32_misc_h

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <malloc.h>

typedef int pid_t;

struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
};

struct timeval;
struct timezone;

int     chdir(const char* path);
pid_t   getpid(void);
char*	getcwd(char* buf, size_t sz);
int     mkdir(const char* name, int mode);
int     gettimeofday(struct timeval* tv, struct timezone* tz);
int     nanosleep(const struct timespec* req, struct timespec* rem);
int     setenv(const char* name, const char* value, int overwrite);

void filetime_to_timeval(FILETIME* ft, struct timeval* tv);

#define snprintf _snprintf

#ifdef _MSC_VER
#define strdup _strdup
#define alloca _alloca
#define stricmp _stricmp
#define strnicmp _strnicmp
#define wcsicmp _wcsicmp
#define wcsnicmp _wcsnicmp

#elif defined(__GNUC__)
_CRTIMP char* __cdecl __MINGW_NOTHROW	strdup (const char*) __MINGW_ATTRIB_MALLOC;
_CRTIMP int __cdecl __MINGW_NOTHROW	stricmp (const char*, const char*);
_CRTIMP int __cdecl __MINGW_NOTHROW	strnicmp (const char*, const char*, size_t);
_CRTIMP int __cdecl __MINGW_NOTHROW	wcsicmp (const wchar_t*, const wchar_t*);
_CRTIMP int __cdecl __MINGW_NOTHROW	wcsnicmp (const wchar_t*, const wchar_t*, size_t);
#define alloca(x) __builtin_alloca((x))
#endif

#endif


