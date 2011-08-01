#ifndef _KNOT_COMMON_GENERAL_TREE_H_
#define _KNOT_COMMON_GENERAL_TREE_H_

#include "common/tree.h"

typedef TREE_HEAD(tree, general_tree_node) general_avl_tree_t;

/* Define tree with void * nodes */
struct general_tree_node {
	TREE_ENTRY(general_tree_node) avl;
	int (*cmp_func)(void *n1,
	                void *n2);
	void *data;
};


struct general_tree {
	int (*cmp_func)(void *n1,
	                void *n2);
	general_avl_tree_t *tree;
};

typedef struct general_tree general_tree_t;

general_tree_t *gen_tree_new(int (*cmp_func)(void *p1, void *p2));

int gen_tree_add(general_tree_t *tree,
                 void *node);

void *gen_tree_find(general_tree_t *tree,
                    void *what);

void gen_tree_apply_inorder(general_tree_t *tree,
                            void (*app_func)(void *node, void *data),
                            void *data);

void gen_tree_destroy(general_tree_t **tree,
                      void (*dest_func)(void *node, void *data));

#endif // _KNOT_COMMON_GENERAL_TREE_H_