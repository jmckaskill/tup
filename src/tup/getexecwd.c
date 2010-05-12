/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "getexecwd.h"
#include "compat.h"
#include "fd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <compat/win32/misc.h>
#include <ldpreload/dllinject.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static char mycwd[PATH_MAX];
static int check_path(const char *path, const char *file);


#ifdef _WIN32
int init_getexecwd(const char *argv0)
{
	char* slash;

	(void) argv0;
	if (GetModuleFileNameA(NULL, mycwd, PATH_MAX - 1) == 0)
		return -1;

	mycwd[PATH_MAX - 1] = '\0';
	slash = strrchr(mycwd, '\\');
	if (slash) {
		*slash = '\0';
	}

	tup_inject_setexecdir(mycwd);

	return 0;
}

#else
int init_getexecwd(const char *argv0)
{
	char *slash;
	fd_t curfd;
	int rc = -1;

	strcpy(mycwd, argv0);
	slash = strrchr(mycwd, '/');
	if(slash) {
		/* Relative and absolute paths */
		if (fd_open(".", O_RDONLY, &curfd)) {
			perror(".");
			return -1;
		}
		*slash = 0;
		if(chdir(mycwd) < 0) {
			perror("chdir");
			goto out_err;
		}
		if(getcwd(mycwd, sizeof(mycwd)) == NULL) {
			perror("getcwd");
			goto out_err;
		}
		if(fd_chdir(curfd) < 0) {
			perror("fchdir");
			goto out_err;
		}
		rc = 0;
out_err:
		fd_close(curfd);
	} else {
		/* Use $PATH */
		char *path;
		char *colon;
		char *p;

		path = getenv("PATH");
		if(!path) {
			fprintf(stderr, "Unable to get PATH environment.\n");
			return -1;
		}

		p = path;
		while(rc == -1 && (colon = strchr(p, ':')) != NULL) {
			*colon = 0;
			rc = check_path(p, argv0);
			*colon = ':';
			p = colon + 1;
		}
		if(rc == -1)
			rc = check_path(p, argv0);
	}

	return rc;
}

static int check_path(const char *path, const char *file)
{
	struct stat buf;
	unsigned int len;

	len = snprintf(mycwd, sizeof(mycwd), "%s/%s", path, file);
	if(len >= sizeof(mycwd)) {
		fprintf(stderr, "Unable to fit path in mycwd buffer.\n");
		goto out_err;
	}

	if(stat(mycwd, &buf) < 0)
		goto out_err;
	if(S_ISREG(buf.st_mode)) {
		strcpy(mycwd, path);
		return 0;
	}
out_err:
	mycwd[0] = 0;
	return -1;
}
#endif

const char *getexecwd(void)
{
	return mycwd;
}

