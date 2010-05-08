/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_getexecwd_h
#define tup_getexecwd_h

/** Initialize the executable's working directory path (the location of the
 * actual executable.
 */
int init_getexecwd(const char *argv0);

/** Get the executable's path. Must call init_getexecwd sometime beforehand,
 * otherwise you'll just get an empty string.
 */
const char *getexecwd(void);

#endif
