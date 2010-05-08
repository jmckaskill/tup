/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_path_h
#define tup_path_h

#include "tupid.h"
#include "fd.h"

struct rb_root;

int watch_path(tupid_t dt, fd_t dfd, const char *file, struct rb_root *tree,
	       int (*callback)(tupid_t newdt, fd_t dfd, const char *file));
int tup_scan(void);

#endif

