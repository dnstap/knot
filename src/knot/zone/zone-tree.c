/*  Copyright (C) 2011 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "knot/zone/zone-tree.h"
#include "knot/zone/node.h"
#include "common/debug.h"
#include "common/hattrie/hat-trie.h"

/*----------------------------------------------------------------------------*/
/* API functions                                                              */
/*----------------------------------------------------------------------------*/

knot_zone_tree_t* knot_zone_tree_create()
{
	return hattrie_create();
}

/*----------------------------------------------------------------------------*/

size_t knot_zone_tree_weight(const knot_zone_tree_t* tree)
{
	return hattrie_weight(tree);
}

int knot_zone_tree_is_empty(const knot_zone_tree_t *tree)
{
	return knot_zone_tree_weight(tree) == 0;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_insert(knot_zone_tree_t *tree, zone_node_t *node)
{
	if (tree == NULL) {
		return KNOT_EINVAL;
	}

	assert(tree && node && node->owner);
	uint8_t lf[KNOT_DNAME_MAXLEN];
	knot_dname_lf(lf, node->owner, NULL);

	*hattrie_get(tree, (char*)lf+1, *lf) = node;
	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_find(knot_zone_tree_t *tree, const knot_dname_t *owner,
                          const zone_node_t **found)
{
	if (owner == NULL || found == NULL) {
		return KNOT_EINVAL;
	}

	return knot_zone_tree_get(tree, owner, (zone_node_t **)found);
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_get(knot_zone_tree_t *tree, const knot_dname_t *owner,
                         zone_node_t **found)
{
	if (owner == NULL) {
		return KNOT_EINVAL;
	}

	if (knot_zone_tree_is_empty(tree)) {
		return KNOT_ENONODE;
	}

	uint8_t lf[KNOT_DNAME_MAXLEN];
	knot_dname_lf(lf, owner, NULL);

	value_t *val = hattrie_tryget(tree, (char*)lf+1, *lf);
	if (val == NULL) {
		*found = NULL;
	} else {
		*found = (zone_node_t*)(*val);
	}

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_find_less_or_equal(knot_zone_tree_t *tree,
                                        const knot_dname_t *owner,
                                        const zone_node_t **found,
                                        const zone_node_t **previous)
{
	if (owner == NULL || found == NULL || previous == NULL) {
		return KNOT_EINVAL;
	}

	zone_node_t *f = NULL, *p = NULL;
	int ret = knot_zone_tree_get_less_or_equal(tree, owner, &f, &p);

	*found = f;
	*previous = p;

	return ret;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_get_less_or_equal(knot_zone_tree_t *tree,
                                       const knot_dname_t *owner,
                                       zone_node_t **found,
                                       zone_node_t **previous)
{
	if (owner == NULL || found == NULL || previous == NULL) {
		return KNOT_EINVAL;
	}

	if (knot_zone_tree_is_empty(tree)) {
		return KNOT_ENONODE;
	}

	uint8_t lf[KNOT_DNAME_MAXLEN];
	knot_dname_lf(lf, owner, NULL);

	value_t *fval = NULL;
	int ret = hattrie_find_leq(tree, (char*)lf+1, *lf, &fval);
	if (fval) {
		*found = (zone_node_t *)(*fval);
	}
	int exact_match = 0;
	if (ret == 0) {
		if (fval) {
			*previous = (*found)->prev;
		}
		exact_match = 1;
	} else if (ret < 0) {
		*previous = *found;
		*found = NULL;
	} else if (ret > 0) {
		/* Previous should be the rightmost node.
		 * For regular zone it is the node left of apex, but for some
		 * cases like NSEC3, there is no such sort of thing (name wise).
		 */
		/*! \todo We could store rightmost node in zonetree probably. */
		hattrie_iter_t *i = hattrie_iter_begin(tree, 1);
		*previous = *(zone_node_t **)hattrie_iter_val(i); /* leftmost */
		*previous = (*previous)->prev; /* rightmost */
		*found = NULL;
		hattrie_iter_free(i);
	}

	/* Previous node for proof must be non-empty and authoritative. */
	if (*previous &&
	    ((*previous)->rrset_count == 0 || (*previous)->flags & NODE_FLAGS_NONAUTH)) {
		*previous = (*previous)->prev;
	}

dbg_zone_exec_detail(
		char *name = knot_dname_to_str(owner);
		char *name_f = (*found != NULL)
			? knot_dname_to_str((*found)->owner)
			: "none";

		dbg_zone_detail("Searched for owner %s in zone tree.\n",
				name);
		dbg_zone_detail("Exact match: %d\n", exact_match);
		dbg_zone_detail("Found node: %p: %s.\n", *found, name_f);
		dbg_zone_detail("Previous node: %p.\n", *previous);

		free(name);
		if (*found != NULL) {
			free(name_f);
		}
);

	return exact_match;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_remove(knot_zone_tree_t *tree,
                            const knot_dname_t *owner,
                          zone_node_t **removed)
{
	if (owner == NULL) {
		return KNOT_EINVAL;
	}

	if (knot_zone_tree_is_empty(tree)) {
		return KNOT_ENONODE;
	}

	uint8_t lf[KNOT_DNAME_MAXLEN];
	knot_dname_lf(lf, owner, NULL);

	value_t *rval = hattrie_tryget(tree, (char*)lf+1, *lf);
	if (rval == NULL) {
		return KNOT_ENOENT;
	} else {
		*removed = (zone_node_t *)(*rval);
	}


	hattrie_del(tree, (char*)lf+1, *lf);
	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_apply_inorder(knot_zone_tree_t *tree,
                                 knot_zone_tree_apply_cb_t function,
                                 void *data)
{
	if (function == NULL) {
		return KNOT_EINVAL;
	}

	if (knot_zone_tree_is_empty(tree)) {
		return KNOT_EOK;
	}

	int result = KNOT_EOK;

	hattrie_iter_t *i = hattrie_iter_begin(tree, 1);
	while(!hattrie_iter_finished(i)) {
		result = function((zone_node_t **)hattrie_iter_val(i), data);
		if (result != KNOT_EOK) {
			break;
		}
		hattrie_iter_next(i);
	}
	hattrie_iter_free(i);

	return result;
}

/*----------------------------------------------------------------------------*/

int knot_zone_tree_apply(knot_zone_tree_t *tree,
                         knot_zone_tree_apply_cb_t function,
                         void *data)
{
	if (function == NULL) {
		return KNOT_EINVAL;
	}

	if (knot_zone_tree_is_empty(tree)) {
		return KNOT_EOK;
	}

	return hattrie_apply_rev(tree, (int (*)(value_t*,void*))function, data);
}

/*----------------------------------------------------------------------------*/

void knot_zone_tree_free(knot_zone_tree_t **tree)
{
	if (tree == NULL || *tree == NULL) {
		return;
	}
	hattrie_free(*tree);
	*tree = NULL;
}

/*----------------------------------------------------------------------------*/

static int knot_zone_tree_free_node(zone_node_t **node, void *data)
{
	UNUSED(data);
	if (node) {
		node_free(node);
	}
	return KNOT_EOK;
}

void knot_zone_tree_deep_free(knot_zone_tree_t **tree)
{
	if (tree == NULL || *tree == NULL) {
		return;
	}

	knot_zone_tree_apply(*tree, knot_zone_tree_free_node, NULL);
	knot_zone_tree_free(tree);
}
