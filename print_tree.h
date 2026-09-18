/*
 * ncursys - A terminal-based tool writen in curses to visualize hard disk usage.
 * Copyright (C) 2026 Dimitris Trivizakis <dim.trivizakis@yandex.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef print_tree_h
#define print_tree_h

#include <stdio.h>
#include <glib.h>
#include "print_node.h"

typedef struct print_tree {
    GNode *root;
    unsigned int total_nodes;   // root is not included
} print_tree, *print_tree_ptr;

/**
 * Initialize tree and return pointer.
 * @return Created tree pointer.
 */
print_tree *print_tree_init(void);

/**
 * Add node under parent in tree.
 *
 * @param tree Pointer to tree.
 * @param parent Parent node.
 * @param pn Node to add.
 * @return Added node.
 */
GNode * print_tree_append(print_tree **tree, GNode *parent, print_node *pn);

/**
 * Destroy tree, free allocated space..
 *
 * @param tree Pointer to tree to destroy.
 */
void print_tree_destroy(print_tree *tree);

/**
 * Sort all children of tree node..
 *
 * @param node The node of which children must be sorted.
 */
void print_tree_sort_children(GNode *node);

/**
 * Find node by it's v_index.
 *
 * @param tree Pointer to tree.
 * @param v_index Index when flatten to search for.
 * @return print_node if found, NULL otherwise.
 */
print_node * print_tree_get_node_by_v_index(print_tree *tree, int* v_index);

/**
 * Find node by it's v_index.
 *
 * @param tree Pointer to tree.
 * @param v_index Index when flatten to search for.
 * @return Tree node if found, NULL otherwise.
 */
GNode * print_tree_get_tree_node_by_v_index(print_tree *tree, int* v_index);

/**
 * Get number of descendants for given node.
 *
 * @param node Node to get descendants number from.
 */
int print_tree_n_descendants(GNode *node);

/**
 * Clear all descendants of node.
 *
 * @param node Node to clear descendants from.
 */
void print_tree_remove_descendants(GNode *node);

#endif /* print_tree_h */
