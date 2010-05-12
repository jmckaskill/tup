/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
#include <compat/win32/misc.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef _WIN32
#define is_path_sep(str) ((str)[0] == '/' || (str)[0] == '\\' || (str)[0] == ':' || ((str)[0] != '\0' && (str)[1] == ':'))
#define is_path_abs(str) (is_path_sep(str) || ((str)[0] == '\0' && (str)[1] == ':'))
#define path_sep '\\'
#define path_sep_str "\\"
#else
#error
#define is_path_sep(ch) (ch == '/')
#define is_path_abs(str) is_path_sep(str)
#define path_sep '/'
#define path_sep_str "/"
#endif

#endif
