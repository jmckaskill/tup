#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>
#include <stdlib.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef _GNU_SOURCE
char* strdup(const char* str);
int snprintf(char* str, size_t size, const char* format, ...);
int setenv(const char* name, const char* value, int overwrite);
#endif

#endif
