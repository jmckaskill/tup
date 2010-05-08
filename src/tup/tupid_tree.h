/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_tupid_tree
#define tup_tupid_tree

#include "linux/rbtree.h"
#include "tupid.h"

struct tupid_tree {
	struct rb_node rbn;
	tupid_t tupid;
};

struct tree_entry {
	struct tupid_tree tnode;
	int type;
};

struct tupid_tree *tupid_tree_search(struct rb_root *root, tupid_t tupid);
int tupid_tree_insert(struct rb_root *root, struct tupid_tree *data);
int tupid_tree_add(struct rb_root *root, tupid_t tupid);
int tupid_tree_add_dup(struct rb_root *root, tupid_t tupid);
int tupid_tree_copy(struct rb_root *dest, struct rb_root *src);
void tupid_tree_remove(struct rb_root *root, tupid_t tupid);
static inline void tupid_tree_rm(struct rb_root *root, struct tupid_tree *tt)
{
	rb_erase(&tt->rbn, root);
}
void free_tupid_tree(struct rb_root *root);
int tree_entry_add(struct rb_root *tree, tupid_t tupid, int type, int *count);
void tree_entry_remove(struct rb_root *tree, tupid_t tupid, int *count);
#define tupid_tree_for_each(_tupid, rbn, tree) \
	for(rbn = rb_first(tree), _tupid = rbn ? rb_entry(rbn, struct tupid_tree, rbn)->tupid : -1; \
	    rbn; \
	    rbn = rb_next(rbn), _tupid = rbn ? rb_entry(rbn, struct tupid_tree, rbn)->tupid : -1)

#endif
