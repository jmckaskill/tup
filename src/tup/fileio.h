/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"
#include "fd.h"
#include "linux/list.h"
#include "linux/rbtree.h"
#include <time.h>

#define TUP_CONFIG "tup.config"

struct tup_entry;

struct path_element {
	struct list_head list;
	const char *path; /* Not nul-terminated */
	int len;
};

#define PG_HIDDEN 1
#define PG_OUTSIDE_TUP 2
#define PG_ROOT 4
struct pel_group {
	struct list_head path_list;
	int pg_flags;
};

int create_name_file(tupid_t dt, const char *file, time_t mtime,
		     struct tup_entry **entry);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t update_symlink_fileat(tupid_t dt, fd_t dfd, const char *file,
			      time_t mtime, int force);
tupid_t tup_file_mod(tupid_t dt, const char *file);
tupid_t tup_file_mod_mtime(tupid_t dt, const char *file, time_t mtime,
			   int force);
int tup_file_del(tupid_t dt, const char *file, int len);
int tup_file_missing(struct tup_entry *tent);
int tup_del_id_force(tupid_t tupid, int type);
void tup_register_rmdir_callback(void (*callback)(tupid_t tupid));
struct tup_entry *get_tent_dt(tupid_t dt, const char *path);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir,
			  struct path_element **last, struct rb_root *symtree,
			  int sotgv);
tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     struct path_element **last,
			     struct list_head *symlist,
			     struct rb_root *symtree, int sotgv);
int add_node_to_list(tupid_t dt, struct pel_group *pg, struct list_head *list,
		     int sotgv);
int gimme_node_or_make_ghost(tupid_t dt, const char *name,
			     struct tup_entry **entry);

int delete_file(tupid_t dt, const char *name);
int delete_name_file(tupid_t tupid);

int get_path_elements(const char *dir, struct pel_group *pg);
int pg_eq(const struct pel_group *pga, const struct pel_group *pgb);
void del_pel(struct path_element *pel);
void del_pel_list(struct list_head *list);

#endif
