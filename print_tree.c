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

#include "print_tree.h"
#include "print_node.h"
#include "path_data.h"
#include <glib.h>
#include <assert.h>

typedef struct search_data {
    print_node *comparison_print_node;
    GNode *found_node;
} search_data;

typedef struct search_data_v_index {
    int *v_index;
    GNode *found_node;
} search_data_v_index;

/* free heap memory of 'node' */
gboolean free_print_node_memory(GNode *node, gpointer data) {
    
    (void)data; // variable is not used
    
    print_node *pd = (print_node *)node->data;
    
    g_free(pd);
    pd = (print_node *)NULL;
        
    return FALSE; // Continue traversal
}

// Comparison function to find a node by id
static gboolean find_node_by_data_pointer(GNode *node, gpointer data) {
    
    print_node *print_node_data = (print_node *)node->data;
    
    search_data* sd= (search_data *)data;
    
    print_node *print_node_data_to_check = (print_node *)sd->comparison_print_node;
    
    if (print_node_data->node == print_node_data_to_check->node) {
        sd->found_node = node;
        return TRUE; // Node found
    }
    return FALSE; // Continue searching
}

void count_descendants(GNode *node, gpointer data)
{
    int *count = (int *)data;
    (*count)++;  // Increment the count for each child node

    // Recursively count the descendants of each child node
    g_node_children_foreach(node, G_TRAVERSE_ALL, count_descendants, count);
}

// Comparison function to find a node by v_index
static gboolean find_node_by_v_index(GNode *node, gpointer data) {
    
    print_node *print_node_data = (print_node *)node->data;
    int v_index_c = print_node_data->v_index;
    
    search_data_v_index* sd= (search_data_v_index *)data;
    
    int *v_index_to_check = (int *)sd->v_index;
    
    if (v_index_c == *v_index_to_check) {
        sd->found_node = node;
        return TRUE; // Node found
    }
    
    
    return FALSE; // Continue searching
}

gint compare_nodes(gconstpointer a, gconstpointer b) {
    
    const GNode *nodeL = (const GNode *)a;
    const GNode *nodeR = (const GNode *)b;
    
    print_node *pnA = (print_node *)nodeL->data;
    print_node *pnB = (print_node *)nodeR->data;
    
    path_data * pdA = ((path_data *)((GNode *)pnA->node)->data);
    path_data * pdB = ((path_data *)((GNode *)pnB->node)->data);
    
    if (pdA->size > pdB->size)
        return -1;
    else if (pdA->size < pdB->size)
        return 1;
    else
        return strcasecmp(pdA->filename, pdB->filename); // if of same size sort alphabetically
}

void print_tree_sort_children(GNode *node) {
    if (node == NULL) return;
    
    // Convert children to a list
    GList *children = NULL;
    for (GNode *child = node->children; child != NULL; child = child->next) {
        children = g_list_prepend(children, child);
    }
    
    // Sort the list
    children = g_list_sort(children, compare_nodes);
    
    // Detach all children
    g_node_children_foreach(node, G_TRAVERSE_ALL, (GNodeForeachFunc)g_node_unlink, NULL);
    
    for (GList *l = children; l != NULL; l = l->next) {
        GNode *child = (GNode *)l->data;
        g_node_append(node, child);
    }
    
    g_list_free(children);
    children = (GList *)NULL;
    
    // Recursively sort children
    for (GNode *child = node->children; child != NULL; child = child->next) {
        print_tree_sort_children(child);
    }
}

print_tree *print_tree_init(void) {
    
    print_tree *pt = g_new(print_tree, 1);
    if(pt == NULL)
        return NULL;
    
    pt->root = (GNode *)NULL;
    
    return pt;
    
}

GNode * print_tree_append(print_tree **tree, GNode *parent, print_node *pn) {
    GNode *new_node = (GNode *)NULL;
    
    // check if tree is created
    if((*tree)->root == (GNode *)NULL) {
        new_node = (*tree)->root = g_node_new(pn);
    }
    else {
        search_data target;
        target.comparison_print_node = pn;
        target.found_node = NULL;
        g_node_traverse((*tree)->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_node_by_data_pointer, &target);
        if(target.found_node == NULL)
            new_node = g_node_append_data(parent, pn);
    }
    
    return new_node;
    
}

int print_tree_n_descendants(GNode *node)
{
    int count = 0;
    g_node_children_foreach(node, G_TRAVERSE_ALL, count_descendants, &count);
    return count;
}

void print_tree_remove_descendants(GNode *node) {
    
    GNode *child = node->children;
    while (child != NULL) {
        GNode *next = child->next;
        g_node_traverse(child, G_PRE_ORDER, G_TRAVERSE_ALL, -1, (GNodeTraverseFunc)free_print_node_memory, NULL);
        g_node_destroy(child);
        child = next;
    }
    //node->children = NULL; // not sure if necessary or redundant
    
}

print_node * print_tree_get_node_by_v_index(print_tree *tree, int* v_index) {
    search_data_v_index target;
    target.v_index = v_index;
    target.found_node = NULL;
    
    g_node_traverse(tree->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_node_by_v_index, &target);
    
    if(target.found_node != NULL)
        return (print_node *)((target.found_node)->data);
    return (print_node *)NULL;
}

GNode * print_tree_get_tree_node_by_v_index(print_tree *tree, int* v_index) {
    search_data_v_index target;
    target.v_index = v_index;
    target.found_node = NULL;
    
    g_node_traverse(tree->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_node_by_v_index, &target);
    
    if(target.found_node != NULL)
        return target.found_node;
    return (GNode *)NULL;
    
}

void print_tree_destroy(print_tree *tree) {
    g_node_traverse(tree->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, free_print_node_memory, NULL);
    g_node_destroy(tree->root);
    g_free(tree);
    tree = NULL;
}



