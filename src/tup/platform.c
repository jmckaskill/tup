#include "platform.h"

#ifdef __linux__
const char *tup_platform = "linux";
#elif __sun__
const char *tup_platform = "solaris";
#elif __APPLE__
const char *tup_platform = "macosx";
#elif _WIN32
const char *tup_platform = "windows";
#else
#error Unsupported platform. Please add support in tup/platform.c
#endif
