#ifndef tup_debug_h
#define tup_debug_h

#ifdef _WIN32
#include <compat/win32/misc.h>
#else
#include <unistd.h>
#endif

/** Environment variable to set if debugging is enabled. */
#define TUP_DEBUG "tup_debug"
#define DEBUGP if(debug_enabled()) fprintf(stderr, "[34m[%s:%i %s:%i][0m ", debug_string(), getpid(), __FILE__, __LINE__), debug_printf

int debug_enabled(void);
const char *debug_string(void);
void debug_enable(const char *label);
void debug_disable(void);
int debug_printf(const char* format, ...);

#endif
