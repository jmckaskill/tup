/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_vardb_h
#define tup_vardb_h

#include "linux/rbtree.h"
#include "string_tree.h"

struct tup_entry;

struct vardb {
	struct rb_root tree;
	int count;
};

struct var_entry {
	struct string_tree var;
	char *value;
	int vallen;
	struct tup_entry *tent;
};

int vardb_init(struct vardb *v);
int vardb_close(struct vardb *v);
int vardb_set(struct vardb *v, const char *var, const char *value,
	      struct tup_entry *tent);
struct var_entry *vardb_set2(struct vardb *v, const char *var, int varlen,
			     const char *value, struct tup_entry *tent);
int vardb_append(struct vardb *v, const char *var, const char *value);
int vardb_len(struct vardb *v, const char *var, int varlen);
int vardb_copy(struct vardb *v, const char *var, int varlen, char **dest);
struct var_entry *vardb_get(struct vardb *v, const char *var, int varlen);
int vardb_compare(struct vardb *vdba, struct vardb *vdbb,
		  int (*extra_a)(struct var_entry *ve),
		  int (*extra_b)(struct var_entry *ve),
		  int (*same)(struct var_entry *vea, struct var_entry *veb));
void vardb_dump(struct vardb *v);

#endif
