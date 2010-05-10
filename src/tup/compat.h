/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>
#include <stdlib.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef _WIN32
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLIMPORT
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#else
DLLIMPORT int snprintf(char* str, size_t size, const char* format, ...);
#endif

#ifndef _GNU_SOURCE
int setenv(const char* name, const char* value, int overwrite);
#endif

#ifdef _WIN32
#define is_path_sep(str) ((str)[0] == '/' || (str)[0] == '\\' || (str)[0] == ':' || ((str)[0] != '\0' && (str)[1] == ':'))
#define is_path_abs(str) (is_path_sep(str) || ((str)[0] == '\0' && (str)[1] == ':'))
#define path_sep '\\'
#define path_sep_str "\\"
#else
#define is_path_sep(ch) (ch == '/')
#define is_path_abs(str) is_path_sep(str)
#define path_sep '/'
#define path_sep_str "/"
#endif

#endif
