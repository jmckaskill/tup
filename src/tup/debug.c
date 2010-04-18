#include <stdlib.h>
#include "debug.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

static int debugging = 0;
static const char *dstring = NULL;

int debug_enabled(void)
{
	return debugging;
}

const char *debug_string(void)
{
	return dstring;
}

void debug_enable(const char *label)
{
	debugging = 1;
	dstring = label;
}

void debug_disable(void)
{
	debugging = 0;
}

int debug_printf(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    return vfprintf(stderr, format, ap);
}

