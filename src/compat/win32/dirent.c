/* vim: set ts=8 sw=8 sts=8 noet tw=78: */

#include "dirent.h"
#include <windows.h>

struct DIR
{
    HANDLE		finder;
    int			first;
    struct dirent	dirent;
};

DIR* opendir(const char* name)
{
	size_t namesz = strlen(name);
	char* str = (char*) malloc(namesz + 3);
	DIR* d = (DIR*) malloc(sizeof(struct DIR));
	d->first = 1;
	
	memcpy(str, name, namesz);
	str[namesz] = '\\';
	str[namesz + 1] = '*';
	str[namesz + 2] = '\0';

	d->finder = FindFirstFileA(str, &d->dirent.data);
	free(str);

	if (d->finder == INVALID_HANDLE_VALUE) {
		free(d);
		return NULL;
	}

	return d;
}

int closedir(DIR* d)
{
	FindClose(d->finder);
	free(d);
	return 0;
}

struct dirent* readdir(DIR* d)
{
	if (d->first) {
		d->first = 0;
		return &d->dirent;
	} else if (FindNextFileA(d->finder, &d->dirent.data)) {
		return &d->dirent;
	} else {
		return NULL;
	}
}

