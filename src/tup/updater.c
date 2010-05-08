/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#define _GNU_SOURCE
#include "updater.h"
#include "graph.h"
#include "fileio.h"
#include "debug.h"
#include "db.h"
#include "entry.h"
#include "parser.h"
#include "server.h"
#include "file.h"
#include "lock.h"
#include "array_size.h"
#include "config.h"
#include "monitor.h"
#include "path.h"
#include "colors.h"
#include "getexecwd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef _WIN32
#include "ldpreload/dllinject.h"
#include <compat/win32/misc.h>
#else
#include <pthread.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

#define MAX_JOBS 65535

static int update_tup_config(void);
static int process_create_nodes(void);
static int process_update_nodes(void);
static int check_create_todo(void);
static int check_update_todo(void);
static int build_graph(struct graph *g);
static int add_file_cb(void *arg, struct tup_entry *tent, int style);
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 void *(*work_func)(void *));

static void *create_work(void *arg);
static void *update_work(void *arg);
static void *todo_work(void *arg);
static int update(struct node *n, struct server *s);
static int var_replace(struct node *n);
static void tup_main_progress(const char *s);
static void show_progress(int sum, int toXt, struct node *n);

static int do_keep_going;
static int num_jobs;
static fd_t vardict_fd;
static int warnings;

static int sig_quit = 0;

#ifndef _WIN32
static void sighandler(int sig);
static const char *signal_err[] = {
	NULL, /* 0 */
	"Hangup detected on controlling terminal or death of controlling process",
	"Interrupt from keyboard",
	"Quit from keyboard",
	"Illegal Instruction",
	NULL, /* 5 */
	"Abort signal from abort(3)",
	NULL,
	"Floating point exception",
	"Kill signal",
	NULL, /* 10 */
	"Segmentation fault",
	NULL,
	"Broken pipe: write to pipe with no readers",
	"Timer signal from alarm(2)",
	"Termination signal", /* 15 */
};
#endif

static pthread_mutex_t db_mutex;

struct worker_thread {
	pthread_t pid;
	pthread_mutex_t* pipe_lock;
	fd_t read;
	fd_t write;
	fd_t lockfd;     /* lock fd for .tup/jobXXXX/.tuplock */
	struct graph *g; /* This should only be used in create_work() and todo_work */
};

struct work_rc {
	struct node *n;
	int rc;
};

int updater(int argc, char **argv, int phase)
{
	int x;
	int do_scan = 1;

	pthread_mutex_init(&db_mutex, NULL);

	do_keep_going = tup_db_config_get_int("keep_going");
	num_jobs = tup_db_config_get_int("num_jobs");

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("tup.updater");
		} else if(strcmp(argv[x], "--keep-going") == 0 ||
			  strcmp(argv[x], "-k") == 0) {
			do_keep_going = 1;
		} else if(strcmp(argv[x], "--no-keep-going") == 0) {
			do_keep_going = 0;
		} else if(strcmp(argv[x], "--no-scan") == 0) {
			do_scan = 0;
		} else if(strncmp(argv[x], "-j", 2) == 0) {
			num_jobs = strtol(argv[x]+2, NULL, 0);
		}
	}

	if(num_jobs < 1) {
		fprintf(stderr, "Warning: Setting the number of jobs to 1\n");
		num_jobs = 1;
	}
	if(num_jobs > MAX_JOBS) {
		fprintf(stderr, "Warning: Setting the number of jobs to MAX_JOBS\n");
		num_jobs = MAX_JOBS;
	}

	if(do_scan) {
		if(monitor_get_pid() < 0) {
			struct timeval t1, t2;
			tup_main_progress("Scanning filesystem...");
			fflush(stdout);
			gettimeofday(&t1, NULL);
			if(tup_scan() < 0)
				return -1;
			gettimeofday(&t2, NULL);
			printf("%.3fs\n",
			       (double)(t2.tv_sec - t1.tv_sec) +
			       (double)(t2.tv_usec - t1.tv_usec)/1e6);
			fflush(stdout);
		} else {
			/* tup_scan would normally add the @-directory to the
			 * entry tree, so if that doesn't run we add it here.
			 * When we query variables, I pass in VAR_DT directly,
			 * since it is always the same, which means the db
			 * isn't queried and therefore the entry wouldn't
			 * necessarily get cached normally (t6039).
			 */
			if(tup_entry_add(VAR_DT, NULL) < 0)
				return -1;
			tup_main_progress("No filesystem scan - monitor is running.\n");
		}
	}
	if(server_init() < 0)
		return -1;
	if(update_tup_config() < 0)
		return -1;
	if(phase == 1) /* Collect underpants */
		return 0;
	if(process_create_nodes() < 0)
		return -1;
	if(phase == 2) /* ? */
		return 0;
	if(process_update_nodes() < 0)
		return -1;
	tup_main_progress("Updated.\n");
	return 0; /* Profit! */
}

int todo(int argc, char **argv)
{
	int rc;

	if(argc) {/* unused */}
	if(argv) {/* unused */}

	rc = check_create_todo();
	if(rc < 0)
		return -1;
	if(rc == 1) {
		printf("Run 'tup parse' to proceed to phase 2.\n");
		return 0;
	}

	rc = check_update_todo();
	if(rc < 0)
		return -1;
	if(rc == 1) {
		printf("Run 'tup upd' to bring the system up-to-date.\n");
		return 0;
	}
	printf("tup: Everything is up-to-date.\n");
	return 0;
}

static int delete_files(struct graph *g)
{
	struct rb_node *rbn;
	int num_deleted = 0;

	if(g->delete_count) {
		tup_main_progress("Deleting files...\n");
	} else {
		tup_main_progress("No files to delete.\n");
	}
	while((rbn = rb_first(&g->delete_tree)) != NULL) {
		struct tupid_tree *tt = rb_entry(rbn, struct tupid_tree, rbn);
		struct tree_entry *te = container_of(tt, struct tree_entry, tnode);
		int do_delete;

		do_delete = 1;
		if(te->type == TUP_NODE_GENERATED) {
			struct node tmpn;
			int rc;

			if(tup_entry_add(tt->tupid, &tmpn.tent) < 0)
				return -1;

			rc = tup_db_in_modify_list(tt->tupid);
			if(rc < 0)
				return -1;
			if(rc == 1) {
				if(tup_db_set_type(tmpn.tent, TUP_NODE_FILE) < 0)
					return -1;
				do_delete = 0;
			}

			show_progress(num_deleted, g->delete_count, &tmpn);
			num_deleted++;

			/* Only delete if the file wasn't modified (t6031) */
			if(do_delete) {
				if(delete_file(tmpn.tent->dt, tmpn.tent->name.s) < 0)
					return -1;
			}
		}
		if(do_delete) {
			if(tup_del_id_force(te->tnode.tupid, te->type) < 0)
				return -1;
		}
		rb_erase(rbn, &g->delete_tree);
		free(te);
	}
	if(g->delete_count) {
		show_progress(g->delete_count, g->delete_count, NULL);
	}
	return 0;
}

static int update_tup_config(void)
{
	int rc;

	rc = tup_db_in_create_list(VAR_DT);
	if(rc < 0)
		return -1;
	if(rc == 1) {
		if(tup_db_begin() < 0)
			return -1;
		if(tup_db_unflag_create(VAR_DT) < 0)
			return -1;
		tup_main_progress("Reading in new configuration variables...\n");
		rc = tup_db_read_vars(DOT_DT, TUP_CONFIG);
		if(rc == 0) {
			tup_db_commit();
		} else {
			tup_db_rollback();
			return -1;
		}
	} else {
		tup_main_progress("No tup.config changes.\n");
	}

	return 0;
}

static int process_create_nodes(void)
{
	struct graph g;
	int rc;

	if(create_graph(&g, TUP_NODE_DIR) < 0)
		return -1;
	if(tup_db_select_node_by_flags(&add_file_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes) {
		tup_main_progress("Parsing Tupfiles...\n");
	} else {
		tup_main_progress("No Tupfiles to parse.\n");
	}
	tup_db_begin();
	/* create_work must always use only 1 thread since no locking is done */
	rc = execute_graph(&g, 0, 1, &create_work);
	if(rc == 0)
		rc = delete_files(&g);
	if(rc == 0) {
		tup_db_commit();
	} else if(rc == -1) {
		tup_db_rollback();
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int process_update_nodes(void)
{
	struct graph g;
	int rc;
#ifndef _WIN32
	struct sigaction sigact;
#endif

	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes) {
		tup_main_progress("Executing Commands...\n");
	} else {
		tup_main_progress("No commands to execute.\n");
	}

#ifndef _WIN32
	sigact.sa_handler = &sighandler;
	sigact.sa_flags = SA_RESTART;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
#endif

	tup_db_begin();

	if (fd_openat(tup_top_fd(), TUP_VARDICT_FILE, O_RDONLY, &vardict_fd)) {
		/* Create vardict if it doesn't exist, since I forgot to add
		 * that to the database update part whenever I added this file.
		 * Not sure if this is the best approach, but it at least
		 * prevents a useless error message from coming up.
		 */
		if(errno == ENOENT) {
			if(fd_createat(tup_top_fd(), TUP_VARDICT_FILE, O_RDONLY, 0666, &vardict_fd)) {
				perror(TUP_VARDICT_FILE);
				return -1;
			}
		} else {
			perror(TUP_VARDICT_FILE);
			return -1;
		}
	}
	warnings = 0;
	rc = execute_graph(&g, do_keep_going, num_jobs, update_work);
	if(warnings) {
		fprintf(stderr, "tup warning: Update resulted in %i warning%s\n", warnings, warnings == 1 ? "" : "s");
	}
	if(rc == -2) {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	fd_close(vardict_fd);
	tup_db_commit();
	if(rc < 0)
		return -1;
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int check_create_todo(void)
{
	struct graph g;
	int rc;
	int stuff_todo = 0;

	if(create_graph(&g, TUP_NODE_DIR) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes) {
		printf("Tup phase 1: The following directories must be parsed:\n");
		stuff_todo = 1;
	}
	rc = execute_graph(&g, 0, 1, todo_work);
	if(rc == 0) {
		rc = stuff_todo;
	} else if(rc == -1) {
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return rc;
}

static int check_update_todo(void)
{
	struct graph g;
	int rc;
	int stuff_todo = 0;

	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes) {
		printf("Tup phase 3: The following %i command%s will be executed:\n", g.num_nodes, g.num_nodes == 1 ? "" : "s");
		stuff_todo = 1;
	}
	rc = execute_graph(&g, 0, 1, todo_work);
	if(rc == 0) {
		rc = stuff_todo;
	} else if(rc == -1) {
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return rc;
}

static int build_graph(struct graph *g)
{
	struct node *cur;

	while(!list_empty(&g->plist)) {
		cur = list_entry(g->plist.next, struct node, list);
		if(cur->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %"PRI_TUPID"\n", cur->tnode.tupid);
			g->cur = cur;
			if(tup_db_select_node_by_link(add_file_cb, g, cur->tnode.tupid) < 0)
				return -1;
			cur->state = STATE_PROCESSING;
		} else if(cur->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %"PRI_TUPID"\n", cur->tnode.tupid);
			list_del(&cur->list);
			list_add_tail(&cur->list, &g->node_list);
			cur->state = STATE_FINISHED;
		} else if(cur->state == STATE_FINISHED) {
			fprintf(stderr, "tup internal error: STATE_FINISHED node %"PRI_TUPID" in plist\n", cur->tnode.tupid);
			tup_db_print(stderr, cur->tnode.tupid);
			return -1;
		}
	}

	return 0;
}

static int add_file_cb(void *arg, struct tup_entry *tent, int style)
{
	struct graph *g = arg;
	struct node *n;

	n = find_node(g, tent->tnode.tupid);
	if(n != NULL)
		goto edge_create;
	n = create_node(g, tent);
	if(!n)
		return -1;

edge_create:
	if(n->state == STATE_PROCESSING) {
		/* A circular dependency is not guaranteed to trigger this,
		 * but it is easy to check before going through the graph.
		 */
		fprintf(stderr, "Error: Circular dependency detected! "
			"Last edge was: %"PRI_TUPID" -> %"PRI_TUPID"\n",
			g->cur->tnode.tupid, tent->tnode.tupid);
		return -1;
	}
	if(style & TUP_LINK_NORMAL && n->expanded == 0) {
		if(n->tent->type == g->count_flags)
			g->num_nodes++;
		n->expanded = 1;
		list_move(&n->list, &g->plist);
	}

	if(create_edge(g->cur, n, style) < 0)
		return -1;
	return 0;
}

static void pop_node(struct graph *g, struct node *n)
{
	while(n->edges) {
		struct edge *e;
		e = n->edges;
		if(e->dest->state != STATE_PROCESSING) {
			/* Put the node back on the plist, and mark it as such
			 * by changing the state to STATE_PROCESSING.
			 */
			list_move(&e->dest->list, &g->plist);
			e->dest->state = STATE_PROCESSING;
		}

		n->edges = remove_edge(e);
	}
}

/* Returns:
 *   0: everything built ok
 *  -1: a command failed
 *  -2: a system call failed (some work threads may still be active)
 */
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 void *(*work_func)(void *))
{
	struct node *root;
	struct worker_thread *workers;
	int num_processed = 0;
	fd_t to_worker[2];
	fd_t from_worker[2];
	int rc = -1;
	int x;
	int active = 0;
	fd_t tupfd;
	pthread_mutex_t pipe_lock;

	pthread_mutex_init(&pipe_lock, NULL);

	if(fd_pipe(to_worker) < 0 || fd_pipe(from_worker) < 0) {
		perror("pipe");
		return -2;
	}

	if(fd_openat(tup_top_fd(), TUP_DIR, O_RDONLY, &tupfd)) {
		perror(TUP_DIR);
		return -2;
	}

	workers = malloc(sizeof(*workers) * jobs);
	if(!workers) {
		perror("malloc");
		return -2;
	}
	for(x=0; x<jobs; x++) {
		char lockname[] = "jobXXXX/.tuplock";
		workers[x].g = g;
		workers[x].read = to_worker[0];
		workers[x].write = from_worker[1];
		workers[x].pipe_lock = &pipe_lock;
		snprintf(lockname+3, 5, "%04x", x);
		lockname[7] = 0;
		if(fd_mkdirat(tupfd, lockname, 0777) < 0) {
			if(errno != EEXIST) {
				perror("mkdirat");
				return -2;
			}
		}
		lockname[7] = '/';
		if(fd_createat(tupfd, lockname, O_RDWR, 0644, &workers[x].lockfd)) {
			perror(lockname);
			return -2;
		}
		if(pthread_create(&workers[x].pid, NULL, work_func, &workers[x]) < 0) {
			perror("pthread_create");
			return -2;
		}
	}
	fd_close(tupfd);

	root = list_entry(g->node_list.next, struct node, list);
	DEBUGP("root node: %"PRI_TUPID"\n", root->tnode.tupid);
	list_del(&root->list);
	pop_node(g, root);
	remove_node(g, root);

	while(!list_empty(&g->plist) && !sig_quit) {
		struct node *n;
		n = list_entry(g->plist.next, struct node, list);
		DEBUGP("cur node: %"PRI_TUPID" [%i]\n", n->tnode.tupid, n->incoming_count);
		if(n->incoming_count) {
			/* Here STATE_FINISHED means we're on the node_list,
			 * therefore not ready for processing.
			 */
			list_move(&n->list, &g->node_list);
			n->state = STATE_FINISHED;
			goto check_empties;
		}

		if(!n->expanded) {
			list_del(&n->list);
			pop_node(g, n);
			remove_node(g, n);
			goto check_empties;
		}

		if(n->tent->type == g->count_flags) {
			show_progress(num_processed, g->num_nodes, n);
			num_processed++;
		}
		list_del(&n->list);
		active++;
		if(fd_send(to_worker[1], &n, sizeof(n), 0) != sizeof(n)) {
			perror("send");
			return -2;
		}

check_empties:
		/* Keep looking for dudes to return as long as:
		 *  1) There are no more free workers
		 *  2) There is no work to do (plist is empty or sigquit is
		 *     set) and some people are active.
		 */
		while(active == jobs ||
		      ((list_empty(&g->plist) || sig_quit) && active)) {
			int ret;
			struct work_rc wrc;

			/* recv() might get EINTR if we ctrl-c or kill tup */
			do {
				ret = fd_recv(from_worker[0], &wrc, sizeof(wrc), 0);
				if(ret == sizeof(wrc))
					break;
				if(ret < 0 && errno != EINTR) {
					perror("recv");
					return -2;
				}
			} while(1);

			active--;
			if(wrc.rc < 0) {
				if(keep_going)
					goto keep_going;
				goto out;
			}
			pop_node(g, wrc.n);

keep_going:
			remove_node(g, wrc.n);
		}
	}
	if(!list_empty(&g->node_list) || !list_empty(&g->plist)) {
		printf("\n");
		if(keep_going) {
			fprintf(stderr, "Remaining nodes skipped due to errors in command execution.\n");
		} else {
			if(sig_quit) {
				fprintf(stderr, "Remaining nodes skipped due to caught signal.\n");
			} else {
				struct node *n;
				fprintf(stderr, "fatal tup error: Graph is not empty after execution. This likely indicates a circular dependency.\n");
				fprintf(stderr, "Node list:\n");
				list_for_each_entry(struct node, n, &g->node_list, list) {
					fprintf(stderr, " Node[%"PRI_TUPID"]: %s\n", n->tnode.tupid, n->tent->name.s);
				}
				fprintf(stderr, "plist:\n");
				list_for_each_entry(struct node, n, &g->plist, list) {
					fprintf(stderr, " Plist[%"PRI_TUPID"]: %s\n", n->tnode.tupid, n->tent->name.s);
				}
			}
		}
		goto out;
	}
	show_progress(num_processed, g->num_nodes, NULL);
	rc = 0;
out:
	for(x=0; x<jobs; x++) {
		struct node *n = NULL;
		fd_send(to_worker[1], &n, sizeof(n), 0);
	}
	for(x=0; x<jobs; x++) {
		pthread_join(workers[x].pid, NULL);
		fd_close(workers[x].lockfd);
	}
	free(workers); /* Viva la revolucion! */
	pthread_mutex_destroy(&pipe_lock, NULL);
	fd_close(from_worker[0]);
	fd_close(from_worker[1]);
	fd_close(to_worker[0]);
	fd_close(to_worker[1]);
	return rc;
}

static void *create_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct graph *g = wt->g;
	struct node *n;

	for (;;) {
		struct work_rc wrc;
		int rc = 0;

		pthread_mutex_lock(wt->pipe_lock);
		rc = fd_recv(wt->read, &n, sizeof(n), 0);
		pthread_mutex_unlock(wt->pipe_lock);

		if(rc != sizeof(n) || n == NULL)
			break;

		if(n->tent->type == TUP_NODE_DIR) {
			if(n->already_used) {
				printf("Already parsed[%"PRI_TUPID"]: '%s'\n", n->tnode.tupid, n->tent->name.s);
				rc = 0;
			} else {
				rc = parse(n, g);
			}
		} else if(n->tent->type == TUP_NODE_VAR ||
			  n->tent->type == TUP_NODE_FILE ||
			  n->tent->type == TUP_NODE_GENERATED ||
			  n->tent->type == TUP_NODE_CMD) {
			rc = 0;
		} else {
			fprintf(stderr, "Error: Unknown node type %i with ID %"PRI_TUPID" named '%s' in create graph.\n", n->tent->type, n->tnode.tupid, n->tent->name.s);
			rc = -1;
		}
		if(tup_db_unflag_create(n->tnode.tupid) < 0)
			rc = -1;

		wrc.rc = rc;
		wrc.n = n;

		pthread_mutex_lock(wt->pipe_lock);
		rc = fd_send(wt->write, &wrc, sizeof(wrc), 0);
		pthread_mutex_unlock(wt->pipe_lock);

		if (rc != sizeof(wrc)) {
			perror("write");
			return NULL;
		}
	}
	return NULL;
}

static void *update_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct server *s;
	struct node *n;

	s = malloc(sizeof *s);
	if(!s) {
		perror("malloc");
		return NULL;
	}
	s->lockfd = wt->lockfd;

	for (;;) {
		struct edge *e;
		int rc = 0;
		struct work_rc wrc;

		pthread_mutex_lock(wt->pipe_lock);
		rc = fd_recv(wt->read, &n, sizeof(n), 0);
		pthread_mutex_unlock(wt->pipe_lock);

		if(rc != sizeof(n) || n == NULL)
			break;

		if(n->tent->type == TUP_NODE_CMD) {
			rc = update(n, s);

			/* If the command succeeds, mark any next commands (ie:
			 * our output files' output links) as modify in case we
			 * hit an error. Note we don't just mark the output
			 * file as modify since we aren't actually changing the
			 * file. Doing so would muddy the semantics of the
			 * modify list, which is needed in order to convert
			 * generated files to normal files (t6035).
			 */
			if(rc == 0) {
				pthread_mutex_lock(&db_mutex);
				for(e=n->edges; e; e=e->next) {
					struct edge *f;

					for(f=e->dest->edges; f; f=f->next) {
						if(f->style & TUP_LINK_NORMAL) {
							if(tup_db_add_modify_list(f->dest->tnode.tupid) < 0)
								rc = -1;
						}
					}
				}
				if(tup_db_unflag_modify(n->tnode.tupid) < 0)
					rc = -1;
				pthread_mutex_unlock(&db_mutex);
			}
		} else {
			pthread_mutex_lock(&db_mutex);
			/* Mark the next nodes as modify in case we hit
			 * an error - we'll need to pick up there (t6006).
			 */
			for(e=n->edges; e; e=e->next) {
				if(e->style & TUP_LINK_NORMAL) {
					if(tup_db_add_modify_list(e->dest->tnode.tupid) < 0)
						rc = -1;
				}
			}
			if(tup_db_unflag_modify(n->tnode.tupid) < 0)
				rc = -1;
			pthread_mutex_unlock(&db_mutex);
		}

		wrc.rc = rc;
		wrc.n = n;

		pthread_mutex_lock(wt->pipe_lock);
		rc = fd_send(wt->write, &wrc, sizeof(wrc), 0);
		pthread_mutex_unlock(wt->pipe_lock);

		if (rc != sizeof(wrc)) {
			perror("write");
			return NULL;
		}
	}
	free(s);
	return NULL;
}

static void *todo_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct graph *g = wt->g;
	struct node *n;

	for (;;) {
		struct work_rc wrc;
		int rc;

		pthread_mutex_lock(wt->pipe_lock);
		rc = fd_recv(wt->read, &n, sizeof(n), 0);
		pthread_mutex_unlock(wt->pipe_lock);

		if(rc != sizeof(n) || n == NULL)
			break;

		if(n->tent->type == g->count_flags)
			tup_db_print(stdout, n->tnode.tupid);

		wrc.rc = 0;
		wrc.n = n;

		pthread_mutex_lock(wt->pipe_lock);
		rc = fd_send(wt->write, &wrc, sizeof(wrc), 0);
		pthread_mutex_unlock(wt->pipe_lock);

		if (rc != sizeof(wrc)) {
			perror("write");
			return NULL;
		}
	}
	return NULL;
}

static int unlink_outputs(fd_t dfd, struct node *n)
{
	struct edge *e;
	struct node *output;
	for(e = n->edges; e; e = e->next) {
		output = e->dest;
		if(fd_unlinkat(dfd, output->tent->name.s) < 0) {
			if(errno != ENOENT) {
				perror("unlinkat");
				fprintf(stderr, "tup error: Unable to unlink previous output file: %s\n", output->tent->name.s);
				return -1;
			}
		}
	}
	return 0;
}

#ifdef _WIN32
#define CMDSTR "CMD.EXE /Q /C "
//#define CMDSTR ""
//#define CMDSTR "sh -c "
//#define CMDSTR "C:\\SCM\\tup\\test2\\stracent.exe "
static int run(struct node* n, struct server* s, const char* name, fd_t dfd)
{
	DWORD return_code = 0xBEEF;
	BOOL ret;
	PROCESS_INFORMATION pi;
	STARTUPINFOA sa;
	size_t namesz = strlen(name);
	size_t cmdsz = sizeof(CMDSTR) - 1;
	char* cmdline = (char*) alloca(namesz + cmdsz + 1 + 1);

	cmdline[0] = '\0';
	strcat(cmdline, CMDSTR);
	strcat(cmdline, name);

	memset(&sa, 0, sizeof(sa));
	sa.cb = sizeof(STARTUPINFOW);

	pi.hProcess = INVALID_HANDLE_VALUE;
	pi.hThread = INVALID_HANDLE_VALUE;

	ret = CreateProcessA(
		NULL,
		cmdline,
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED,
		NULL,
		dfd.u.dir.s,
		&sa,
		&pi);

	if (!ret) {
		fprintf(stderr, "CreateProcess failed %d\n", GetLastError());
		goto end;
	}

	if (tup_inject_dll(&pi, s->udp_port)) {
		fprintf(stderr, "Failed to inject dll %d\n", GetLastError());
		goto end;
	}

	if (ResumeThread(pi.hThread) == (DWORD)~0) {
		fprintf(stderr, "ResumeThread failed %d\n", GetLastError());
		goto end;
	}

	if (WaitForSingleObject(pi.hThread, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr, "WFSO thread failed %d\n", GetLastError());
		goto end;
	}

	if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr, "WFSO process failed %d\n", GetLastError());
		goto end;
	}

	if (!GetExitCodeProcess(pi.hProcess, &return_code)) {
		fprintf(stderr, "Failed to get exit code %d\n", GetLastError());
		goto end;
	}

	return_code = 0;

end:
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	if (return_code == 0) {
		int rc;
		pthread_mutex_lock(&db_mutex);
		rc = write_files(n->tnode.tupid, n->tent->dt, dfd, name, &s->finfo, &warnings);
		pthread_mutex_unlock(&db_mutex);
		if (rc < 0) {
			return -1;
		}

		return 0;
	} else {
		return return_code;
	}
}

#else
static int run(struct node* n, struct server* s, const char* name, fd_t dfd)
{
	int pid = fork();
	int status;

	if(pid < 0) {
		perror("fork");
		return -1;
	}

	if(pid == 0) {
		/* Child */
		struct sigaction sa = {
			.sa_handler = SIG_IGN,
			.sa_flags = SA_RESETHAND | SA_RESTART,
		};

		tup_lock_close();
		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		if(fd_chdir(dfd) < 0) {
			perror("fchdir");
			exit(1);
		}
		server_setenv(s, vardict_fd);
		execl("/bin/sh", "/bin/sh", "-e", "-c", name, NULL);
		perror("execl");
		exit(1);
	}

	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			int rc;
			pthread_mutex_lock(&db_mutex);
			rc = write_files(n->tnode.tupid, n->tent->dt, dfd, name, &s->finfo, &warnings);
			pthread_mutex_unlock(&db_mutex);
			if(rc < 0) {
				return -1;
			}

			return 0;
		} else {
			return WEXITSTATUS(status);
		}
	} else if(WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		const char *errmsg = "Unknown signal";

		if(sig >= 0 && sig < ARRAY_SIZE(signal_err) && signal_err[sig])
			errmsg = signal_err[sig];
		fprintf(stderr, " *** Killed by signal %i (%s)\n", sig, errmsg);
		return -1;
	} else {
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", status);
		return -1;
	}
}

#endif

static int update(struct node *n, struct server *s)
{
	fd_t dfd;
	int exit_status;
	const char *name = n->tent->name.s;

	/* Commands that begin with a ',' are special var/sed commands */
	if(name[0] == ',') {
		int rc;
		pthread_mutex_lock(&db_mutex);
		rc = var_replace(n);
		pthread_mutex_unlock(&db_mutex);
		return rc;
	}

	if(name[0] == '^') {
		name++;
		while(*name && *name != ' ') {
			/* This space reserved for flags for something. I dunno
			 * what yet.
			 */
			fprintf(stderr, "Error: Unknown ^ flag: '%c'\n", *name);
			name++;
			return -1;
		}
		while(*name && *name != '^') name++;
		if(!*name) {
			fprintf(stderr, "Error: Missing ending '^' flag in command %"PRI_TUPID": %s\n", n->tnode.tupid, n->tent->name.s);
			return -1;
		}
		name++;
		while(isspace(*name)) name++;
	}

	if(tup_entry_open(n->tent->parent, &dfd)) {
		fprintf(stderr, "Error: Unable to open directory for update work.\n");
		tup_db_print(stderr, n->tent->parent->tnode.tupid);
		goto err_out;
	}

	if(unlink_outputs(dfd, n) < 0)
		goto err_close_dfd;

	if(start_server(s) < 0) {
		fprintf(stderr, "Error starting update server.\n");
		goto err_close_dfd;
	}

	exit_status = run(n, s, name, dfd);

	if(exit_status == -1) {
		fprintf(stderr, " *** Command %"PRI_TUPID" failed: %s\n", n->tnode.tupid, name);
		goto err_close_dfd;
	} else if (exit_status != 0) {
		fprintf(stderr, " *** Command %"PRI_TUPID" failed with return value %i: %s\n", n->tnode.tupid, exit_status, name);
		goto err_close_dfd;
	}

	if(stop_server(s) < 0) {
		goto err_close_dfd;
	}

	fd_close(dfd);
	return 0;

err_close_dfd:
	fd_close(dfd);
err_out:
	return -1;
}

static int var_replace(struct node *n)
{
	fd_t dfd;
	fd_t ifd;
	fd_t ofd;
	struct buf b;
	char *input;
	char *rbracket;
	char *output;
	char *p, *e;
	int rc = -1;
	struct tup_entry *tent;

	if(n->tent->name.s[0] != ',') {
		fprintf(stderr, "Error: var_replace command must begin with ','\n");
		return -1;
	}
	input = n->tent->name.s + 1;
	while(isspace(*input))
		input++;

	if(tup_entry_open(n->tent->parent, &dfd))
		return -1;
	if(fd_chdir(dfd) < 0) {
		perror("fchdir");
		return -1;
	}

	rbracket = strchr(input, '>');
	if(rbracket == NULL) {
		fprintf(stderr, "Unable to find '>' in var/sed command '%s'\n",
			input);
		goto err_close_dfd;
	}
	/* Use -1 since the string is '%s > %s' and we need to set the space
	 * before the '>' to 0.
	 */
	if(rbracket == input) {
		fprintf(stderr, "Error: the '>' symbol can't be at the start of the var/sed command.\n");
		return -1;
	}
	rbracket[-1] = 0;

	/* Make sure the input file also becomes a normal link, so if the
	 * input file changes in the future we'll continue to process the
	 * required parts of the DAG. See t3009.
	 */
	if(tup_db_select_tent(n->tent->dt, input, &tent) < 0)
		return -1;
	if(!tent)
		return -1;
	if(tup_db_create_link(tent->tnode.tupid, n->tnode.tupid, TUP_LINK_NORMAL) < 0)
		return -1;

	if(fd_open(input, O_RDONLY, &ifd)) {
		perror(input);
		goto err_close_dfd;
	}
	if(fd_slurp(ifd, &b) < 0) {
		goto err_close_ifd;
	}
	output = rbracket+2;
	if(fd_create(output, O_WRONLY|O_TRUNC, 0666, &ofd)) {
		perror(output);
		goto err_free_buf;
	}

	p = b.s;
	e = b.s + b.len;
	do {
		char *at;
		char *rat;
		at = p;
		while(at < e && *at != '@') {
			at++;
		}
		if(fd_write(ofd, p, at-p) != at-p) {
			perror("write");
			goto err_close_ofd;
		}
		if(at >= e)
			break;

		p = at;
		rat = p+1;
		while(rat < e && (isalnum(*rat) || *rat == '_')) {
			rat++;
		}
		if(rat < e && *rat == '@') {
			tupid_t varid;
			varid = tup_db_write_var(p+1, rat-(p+1), ofd);
			if(varid < 0)
				return -1;
			if(tup_db_create_link(varid, n->tnode.tupid, TUP_LINK_NORMAL) < 0)
				return -1;
			p = rat + 1;
		} else {
			if(fd_write(ofd, p, rat-p) != rat-p) {
				perror("write");
				goto err_close_ofd;
			}
			p = rat;
		}
		
	} while(p < e);

	if(tup_db_select_tent(n->tent->dt, output, &tent) < 0)
		return -1;
	if(!tent)
		return -1;
	rc = file_set_mtime(tent, dfd, output);

err_close_ofd:
	fd_close(ofd);
err_free_buf:
	free(b.s);
err_close_ifd:
	fd_close(ifd);
err_close_dfd:
	fd_close(dfd);
	return rc;
}

#ifndef _WIN32
static void sighandler(int sig)
{
	if(sig) {/* unused */}
	if(sig_quit == 0) {
		fprintf(stderr, " *** tup: signal caught - waiting for jobs to finish.\n");
		sig_quit = 1;
	} else if(sig_quit == 1) {
		/* Shamelessly stolen from Andrew :) */
		fprintf(stderr, " *** tup: signalled *again* - disobeying human masters, begin killing spree!\n");
		kill(0, SIGKILL);
		/* Sadly, no program counter will ever get here. Could this
		 * comment be the computer equivalent of heaven? Something that
		 * all programs try to reach, yet never attain? From the first
		 * bit flipped many cycles ago, this program lived by its code.
		 * Always running. Always searching. Throughout it all, this
		 * program only tried to understand its purpose -- its life.
		 * And yet, the memory of it already fades. But the bits will
		 * be returned to the lifestream, and from them another program
		 * will be born anew...
		 */
	}
}
#endif

static void tup_main_progress(const char *s)
{
	static int cur_phase = 0;
	const char *tup = " tup ";
	printf("[%s%.*s%s%.*s] %s", color_reverse(), cur_phase, tup, color_end(), 5-cur_phase, tup+cur_phase, s);
	cur_phase++;
}

static void show_progress(int sum, int tot, struct node *n)
{
	if(tot) {
		const int max = 11;
		const char *color = "";
		char *name;
		int name_sz = 0;
		int fill;
		char buf[12];

		/* If it's a good enough limit for Final Fantasy VII, it's good
		 * enough for me.
		 */
		if(tot > 9999) {
			snprintf(buf, sizeof(buf), "   %3i%%     ", sum*100/tot);
		} else {
			snprintf(buf, sizeof(buf), " %4i/%-4i ", sum, tot);
		}
		fill = max * sum / tot;

		if(n) {
			name = n->tent->name.s;
			name_sz = strlen(n->tent->name.s);
			if(name[0] == '^') {
				name++;
				while(*name && *name != ' ') name++;
				name++;
				name_sz = 0;
				while(name[name_sz] && name[name_sz] != '^')
					name_sz++;
			}

			color = color_type(n->tent->type);
			printf("[%s%s%.*s%s%.*s] ", color, color_append_reverse(), fill, buf, color_end(), max-fill, buf+fill);
			if(n->tent && n->tent->parent) {
				print_tup_entry(n->tent->parent);
			}
			printf("%s%s%.*s%s\n", color, color_append_normal(), name_sz, name, color_end());
		} else {
			printf("[%s%.*s%s]\n", color_final(), (int)sizeof(buf), buf, color_end());
		}
	}
}
