/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_compat_win32_dirent_h
#define tup_compat_win32_dirent_h

#include <windows.h>

typedef struct DIR DIR;
struct dirent
{
	WIN32_FIND_DATAA data;
};

#define d_name data.cFileName

DIR* opendir(const char* name);
int closedir(DIR* d);
struct dirent* readdir(DIR* d);


#endif

