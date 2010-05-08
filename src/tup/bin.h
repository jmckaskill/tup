/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "linux/list.h"
#include "tupid.h"

struct tup_entry;

struct bin_list {
	struct list_head bins;
};

struct bin {
	struct list_head list;
	const char *name;
	struct list_head entries;
};

struct bin_entry {
	struct list_head list;
	char *path;
	int len;
	struct tup_entry *tent;
};

int bin_list_init(struct bin_list *bl);
void bin_list_del(struct bin_list *bl);
struct bin *bin_add(const char *name, struct bin_list *bl);
struct bin *bin_find(const char *name, struct bin_list *bl);
int bin_add_entry(struct bin *b, const char *path, int len,
		  struct tup_entry *tent);
