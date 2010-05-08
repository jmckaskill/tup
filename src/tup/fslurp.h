/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
struct buf {
	char *s;
	int len;
};

int fslurp(int fd, struct buf *b);
int fslurp_null(int fd, struct buf *b);
