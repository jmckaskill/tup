/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "path.h"
#include "flist.h"
#include "fileio.h"
#include "monitor.h"
#include "db.h"
#include "config.h"
#include "entry.h"
#include <stdio.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

int watch_path(tupid_t dt, fd_t dfd, const char *file, struct rb_root *tree,
	       int (*callback)(tupid_t newdt, fd_t dfd, const char *file))
{
	struct flist f = {0, 0, 0};
	struct stat buf;
	tupid_t newdt;

	if(fd_lstatat(dfd, file, &buf) != 0) {
		fprintf(stderr, "tup monitor error: fstatat failed\n");
		perror(file);
		return -1;
	}

	if(S_ISREG(buf.st_mode)) {
		tupid_t tupid;
		tupid = tup_file_mod_mtime(dt, file, buf.st_mtime, 0);
		if(tupid < 0)
			return -1;
		if(tree) {
			tupid_tree_remove(tree, tupid);
		}
		return 0;
	} else if(S_ISLNK(buf.st_mode)) {
		tupid_t tupid;

		tupid = update_symlink_fileat(dt, dfd, file, buf.st_mtime, 0);
		if(tupid < 0)
			return -1;
		if(tree) {
			tupid_tree_remove(tree, tupid);
		}
		return 0;
	} else if(S_ISDIR(buf.st_mode)) {
		fd_t newfd;

		newdt = create_dir_file(dt, file);
		if(tree) {
			tupid_tree_remove(tree, newdt);
		}

		if(callback) {
			if(callback(newdt, dfd, file) < 0)
				return -1;
		}

		if (fd_openat(dfd, file, O_RDONLY, &newfd)) {
			fprintf(stderr, "tup monitor error: Unable to openat() directory.\n");
			perror(file);
			return -1;
		}
		if(fd_chdir(newfd) < 0) {
			perror("fchdir");
			return -1;
		}

		flist_foreach(&f, ".") {
			if(f.filename[0] == '.')
				continue;
			if(watch_path(newdt, newfd, f.filename, tree,
				      callback) < 0)
				return -1;
		}
		fd_close(newfd);
		return 0;
	} else {
		fprintf(stderr, "Error: File '%s' is not regular nor a dir?\n",
			file);
		return -1;
	}
}

int tup_scan(void)
{
	struct rb_root scan_tree = RB_ROOT;
	if(tup_db_scan_begin(&scan_tree) < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", &scan_tree, NULL) < 0)
		return -1;
	if(tup_db_scan_end(&scan_tree) < 0)
		return -1;
	return 0;
}
