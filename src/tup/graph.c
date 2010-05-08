#define _GNU_SOURCE /* TODO: For asprintf */
#include "graph.h"
#include "entry.h"
#include "debug.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct tup_entry root_entry;
static char root_name[] = "root";

struct node *find_node(struct graph *g, tupid_t tupid)
{
	struct tupid_tree *tnode;
	tnode = tupid_tree_search(&g->tree, tupid);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct node, tnode);
}

struct node *create_node(struct graph *g, struct tup_entry *tent)
{
	struct node *n;

	n = malloc(sizeof *n);
	if(!n) {
		perror("malloc");
		return NULL;
	}
	n->edges = NULL;
	n->incoming_count = 0;
	n->tnode.tupid = tent->tnode.tupid;
	n->tent = tent;
	n->state = STATE_INITIALIZED;
	n->already_used = 0;
	n->expanded = 0;
	n->parsing = 0;
	list_add_tail(&n->list, &g->node_list);

	if(tupid_tree_insert(&g->tree, &n->tnode) < 0)
		return NULL;
	return n;
}

void remove_node(struct graph *g, struct node *n)
{
	if(n->edges) {
		DEBUGP("Warning: Node %"PRI_TUPID" still has edges.\n", n->tnode.tupid);
	}
	rb_erase(&n->tnode.rbn, &g->tree);
	free(n);
}

int create_edge(struct node *n1, struct node *n2, int style)
{
	struct edge *e;

	e = malloc(sizeof *e);
	if(!e) {
		perror("malloc");
		return -1;
	}

	e->dest = n2;
	e->style = style;

	e->next = n1->edges;
	n1->edges = e;

	n2->incoming_count++;
	return 0;
}

struct edge *remove_edge(struct edge *e)
{
	struct edge *tmp;
	tmp = e->next;
	e->dest->incoming_count--;
	free(e);
	return tmp;
}

int create_graph(struct graph *g, int count_flags)
{
	root_entry.tnode.tupid = 0;
	root_entry.dt = 0;
	root_entry.sym = -1;
	root_entry.parent = NULL;
	root_entry.symlink = NULL;
	root_entry.type = TUP_NODE_ROOT;
	root_entry.mtime = -1;
	root_entry.name.len = strlen(root_name);
	root_entry.name.s = root_name;
	root_entry.entries.rb_node = NULL;

	INIT_LIST_HEAD(&g->node_list);
	INIT_LIST_HEAD(&g->plist);
	g->delete_tree.rb_node = NULL;
	g->delete_count = 0;

	g->tree.rb_node = NULL;

	g->cur = g->root = create_node(g, &root_entry);
	if(!g->root)
		return -1;
	g->num_nodes = 0;
	g->count_flags = count_flags;
	return 0;
}

int destroy_graph(struct graph *g)
{
	if(g) {/* TODO */}
	return 0;
}

