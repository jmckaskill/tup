/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "fileio.h"
#include "db.h"
#include "entry.h"
#include <stdio.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#endif

int delete_name_file(tupid_t tupid)
{
	if(tup_db_unflag_create(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid) < 0)
		return -1;
	return 0;
}

int delete_file(tupid_t dt, const char *name)
{
	fd_t dirfd;
	int rc = 0;
	int err;

	err = tup_entry_open_tupid(dt, &dirfd);
	if(err == -ENOENT) {
		/* If the directory doesn't exist, the file can't
		 * either
		 */
		return 0;
	} else if (err) {
		return -1;
	}

	if(fd_unlinkat(dirfd, name) < 0) {
		/* Don't care if the file is already gone. */
		if(errno != ENOENT) {
			perror(name);
			rc = -1;
			goto out;
		}
	}

out:
	fd_close(dirfd);
	return rc;
}
